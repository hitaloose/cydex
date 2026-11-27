//! macOS: notificações do NSWorkspace (tela dormiu/acordou, sistema vai dormir/acordou).
//! Só chegam com o NSApplication rodando, o que o Tauri já faz. Tem que ser
//! chamado na thread principal (o `setup` do Tauri roda nela).

use super::{Handler, PowerEvent};
use block2::RcBlock;
use objc2_app_kit::{
    NSWorkspace, NSWorkspaceDidWakeNotification, NSWorkspaceScreensDidSleepNotification,
    NSWorkspaceScreensDidWakeNotification, NSWorkspaceWillSleepNotification,
};
use objc2::rc::Retained;
use objc2::runtime::{NSObjectProtocol, ProtocolObject};
use objc2_foundation::{NSActivityOptions, NSNotification, NSOperationQueue, NSProcessInfo, NSString};
use std::ptr::NonNull;

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

pub fn start(on: Handler) {
    let center = NSWorkspace::sharedWorkspace().notificationCenter();
    let queue = NSOperationQueue::mainQueue();
    // SAFETY: constantes do AppKit, válidas a execução inteira.
    let events = unsafe {
        [
            (NSWorkspaceScreensDidSleepNotification, PowerEvent::ScreenOff),
            (NSWorkspaceWillSleepNotification, PowerEvent::SystemSleep),
            (NSWorkspaceScreensDidWakeNotification, PowerEvent::ScreenOn),
            (NSWorkspaceDidWakeNotification, PowerEvent::SystemWake),
        ]
    };
    for (name, event) in events {
        let on = on.clone();
        let block = RcBlock::new(move |_n: NonNull<NSNotification>| on(event));
        // SAFETY: bloco 'static, chamado na fila principal.
        let token = unsafe {
            center.addObserverForName_object_queue_usingBlock(Some(name), None, Some(&queue), &block)
        };
        // O observador vale enquanto o app viver.
        std::mem::forget(token);
    }
}
