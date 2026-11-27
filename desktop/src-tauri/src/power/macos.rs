//! macOS: notificações do NSWorkspace (tela dormiu/acordou, sistema acordou)
//! e o IOKit pro "vai dormir", que segura o sono até o /sleep sair. As do
//! NSWorkspace só chegam com o NSApplication rodando, o que o Tauri já faz.
//! Tem que ser chamado na thread principal (o `setup` do Tauri roda nela).

use super::{Ack, Handler, PowerEvent};
use block2::RcBlock;
use objc2_app_kit::{
    NSWorkspace, NSWorkspaceDidWakeNotification, NSWorkspaceScreensDidSleepNotification,
    NSWorkspaceScreensDidWakeNotification, NSWorkspaceWillSleepNotification,
};
use objc2::rc::Retained;
use objc2::runtime::{NSObjectProtocol, ProtocolObject};
use objc2_foundation::{NSActivityOptions, NSNotification, NSOperationQueue, NSProcessInfo, NSString};
use std::ffi::c_void;
use std::ptr::NonNull;
use std::sync::OnceLock;

/// Atividade do NSProcessInfo enquanto um envio de energia está no ar. O app
/// só tem bandeja (ActivationPolicy::Accessory) e é candidato ao App Nap, que
/// segura timers e rede justo quando a tela apaga. Termina no `drop`.
pub struct Activity(Retained<ProtocolObject<dyn NSObjectProtocol>>);

// SAFETY: o NSProcessInfo é thread-safe (Send + Sync no objc2) e o token
// opaco só volta pra ele no endActivity.
unsafe impl Send for Activity {}

pub fn begin_activity(reason: &str) -> Activity {
    let reason = NSString::from_str(reason);
    // Não impede o computador de dormir: só tira o app do App Nap.
    let token = NSProcessInfo::processInfo()
        .beginActivityWithOptions_reason(NSActivityOptions::UserInitiatedAllowingIdleSystemSleep, &reason);
    Activity(token)
}

impl Drop for Activity {
    fn drop(&mut self) {
        // SAFETY: o token veio do beginActivityWithOptions_reason acima.
        unsafe { NSProcessInfo::processInfo().endActivity(&self.0) };
    }
}

// ---------- IOKit: segurar o sono até o /sleep sair ----------
//
// O NSWorkspaceWillSleepNotification não segura nada: o Mac dorme e o Wi-Fi
// cai antes do POST sair. O IORegisterForSystemPower dá o
// kIOMessageSystemWillSleep, e o sono espera até o IOAllowPowerChange (ou
// ~30 s, o limite do sistema). Quem chama o allow é o Ack, quando o envio
// termina ou o teto de 5 s passa (lib.rs).

type IoConnect = u32;
type IoObject = u32;
type IoNotificationPortRef = *mut c_void;
type IoServiceInterestCallback = extern "C" fn(refcon: *mut c_void, service: IoObject, message_type: u32, argument: *mut c_void);

#[link(name = "IOKit", kind = "framework")]
extern "C" {
    fn IORegisterForSystemPower(
        refcon: *mut c_void,
        port: *mut IoNotificationPortRef,
        callback: IoServiceInterestCallback,
        notifier: *mut IoObject,
    ) -> IoConnect;
    fn IONotificationPortGetRunLoopSource(port: IoNotificationPortRef) -> *mut c_void;
    fn IOAllowPowerChange(kernel_port: IoConnect, notification_id: isize) -> i32;
}

#[link(name = "CoreFoundation", kind = "framework")]
extern "C" {
    fn CFRunLoopGetMain() -> *mut c_void;
    fn CFRunLoopAddSource(rl: *mut c_void, source: *mut c_void, mode: *const c_void);
    static kCFRunLoopCommonModes: *const c_void;
}

// iokit_common_msg(x) = 0xE0000000 | x (IOKit/IOMessage.h).
const MSG_CAN_SYSTEM_SLEEP: u32 = 0xE000_0270;
const MSG_SYSTEM_WILL_SLEEP: u32 = 0xE000_0280;

static ROOT_PORT: OnceLock<IoConnect> = OnceLock::new();
static IOKIT_HANDLER: OnceLock<Handler> = OnceLock::new();

fn allow(id: isize) {
    if let Some(&port) = ROOT_PORT.get() {
        // SAFETY: porta do IORegisterForSystemPower, id da mensagem recebida.
        unsafe { IOAllowPowerChange(port, id) };
    }
}

extern "C" fn on_system_power(_refcon: *mut c_void, _service: IoObject, message_type: u32, argument: *mut c_void) {
    let id = argument as isize;
    match message_type {
        // Pergunta se pode dormir (sono por inatividade): sempre pode, na hora.
        MSG_CAN_SYSTEM_SLEEP => allow(id),
        // Vai dormir: o sono espera o Ack.
        MSG_SYSTEM_WILL_SLEEP => match IOKIT_HANDLER.get() {
            Some(h) => h(PowerEvent::SystemSleep, Ack::new(move || allow(id))),
            None => allow(id),
        },
        _ => {}
    }
}

/// Registra no IOKit. Falhou: o "vai dormir" volta a vir do NSWorkspace.
fn register_iokit(on: Handler) -> bool {
    if IOKIT_HANDLER.set(on).is_err() {
        return ROOT_PORT.get().is_some();
    }
    let mut port: IoNotificationPortRef = std::ptr::null_mut();
    let mut notifier: IoObject = 0;
    // SAFETY: ponteiros válidos; o callback é 'static e roda no run loop
    // principal, onde a fonte é adicionada (start roda na thread principal).
    unsafe {
        let root = IORegisterForSystemPower(std::ptr::null_mut(), &mut port, on_system_power, &mut notifier);
        if root == 0 || port.is_null() {
            log::warn!("power: IORegisterForSystemPower falhou; o sono não espera o /sleep");
            return false;
        }
        let _ = ROOT_PORT.set(root);
        CFRunLoopAddSource(CFRunLoopGetMain(), IONotificationPortGetRunLoopSource(port), kCFRunLoopCommonModes);
    }
    log::info!("power: IOKit registrado (o sono espera o /sleep, até 5 s)");
    true
}

pub fn start(on: Handler) {
    let iokit = register_iokit(on.clone());
    let center = NSWorkspace::sharedWorkspace().notificationCenter();
    let queue = NSOperationQueue::mainQueue();
    // SAFETY: constantes do AppKit, válidas a execução inteira.
    let mut events = unsafe {
        vec![
            (NSWorkspaceScreensDidSleepNotification, PowerEvent::ScreenOff),
            (NSWorkspaceScreensDidWakeNotification, PowerEvent::ScreenOn),
            (NSWorkspaceDidWakeNotification, PowerEvent::SystemWake),
        ]
    };
    // Com o IOKit, o "vai dormir" vem de lá (o do NSWorkspace não segura o sono).
    if !iokit {
        // SAFETY: idem.
        events.push((unsafe { NSWorkspaceWillSleepNotification }, PowerEvent::SystemSleep));
    }
    for (name, event) in events {
        let on = on.clone();
        let block = RcBlock::new(move |_n: NonNull<NSNotification>| on(event, Ack::none()));
        // SAFETY: bloco 'static, chamado na fila principal.
        let token = unsafe {
            center.addObserverForName_object_queue_usingBlock(Some(name), None, Some(&queue), &block)
        };
        // O observador vale enquanto o app viver.
        std::mem::forget(token);
    }
}
