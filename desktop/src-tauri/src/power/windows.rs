//! Windows: janela invisível que recebe WM_POWERBROADCAST (suspender/retomar)
//! e a mudança de estado do monitor (GUID_CONSOLE_DISPLAY_STATE: tela
//! apagou/acendeu). Janela "message-only" não recebe o broadcast, por isso é
//! uma janela normal que nunca aparece.

use super::{Handler, PowerEvent};
use std::ptr::null_mut;
use std::sync::OnceLock;
use windows_sys::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM};
use windows_sys::Win32::System::LibraryLoader::GetModuleHandleW;
use windows_sys::Win32::System::Power::{RegisterPowerSettingNotification, POWERBROADCAST_SETTING};
use windows_sys::Win32::System::SystemServices::GUID_CONSOLE_DISPLAY_STATE;
use windows_sys::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, DefWindowProcW, DispatchMessageW, GetMessageW, RegisterClassW, TranslateMessage,
    DEVICE_NOTIFY_WINDOW_HANDLE, MSG, PBT_APMRESUMEAUTOMATIC, PBT_APMSUSPEND, PBT_POWERSETTINGCHANGE,
    WM_POWERBROADCAST, WNDCLASSW,
};

static HANDLER: OnceLock<Handler> = OnceLock::new();

fn wide(s: &str) -> Vec<u16> {
    s.encode_utf16().chain(std::iter::once(0)).collect()
}

unsafe extern "system" fn wndproc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    if msg == WM_POWERBROADCAST {
        let event = match wparam as u32 {
            PBT_APMSUSPEND => Some(PowerEvent::SystemSleep),
            PBT_APMRESUMEAUTOMATIC => Some(PowerEvent::SystemWake),
            PBT_POWERSETTINGCHANGE if lparam != 0 => {
                let setting = &*(lparam as *const POWERBROADCAST_SETTING);
                let same = setting.PowerSetting.data1 == GUID_CONSOLE_DISPLAY_STATE.data1
                    && setting.PowerSetting.data2 == GUID_CONSOLE_DISPLAY_STATE.data2
                    && setting.PowerSetting.data3 == GUID_CONSOLE_DISPLAY_STATE.data3
                    && setting.PowerSetting.data4 == GUID_CONSOLE_DISPLAY_STATE.data4;
                // 0 = desligado, 1 = ligado, 2 = escurecido (ignora).
                match (same, setting.Data[0]) {
                    (true, 0) => Some(PowerEvent::ScreenOff),
                    (true, 1) => Some(PowerEvent::ScreenOn),
                    _ => None,
                }
            }
            _ => None,
        };
        if let (Some(e), Some(h)) = (event, HANDLER.get()) {
            h(e, super::Ack::none());
        }
        return 1;
    }
    DefWindowProcW(hwnd, msg, wparam, lparam)
}

pub fn start(on: Handler) {
    if HANDLER.set(on).is_err() {
        return;
    }
    std::thread::spawn(|| unsafe {
        let instance = GetModuleHandleW(null_mut());
        let class = wide("CYDexPowerWatch");
        let mut wc: WNDCLASSW = std::mem::zeroed();
        wc.lpfnWndProc = Some(wndproc);
        wc.hInstance = instance;
        wc.lpszClassName = class.as_ptr();
        RegisterClassW(&wc);
        let title = wide("CYDex");
        let hwnd = CreateWindowExW(
            0,
            class.as_ptr(),
            title.as_ptr(),
            0,
            0,
            0,
            0,
            0,
            null_mut(),
            null_mut(),
            instance,
            null_mut(),
        );
        if hwnd.is_null() {
            return;
        }
        RegisterPowerSettingNotification(hwnd, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);
        let mut msg: MSG = std::mem::zeroed();
        while GetMessageW(&mut msg, null_mut(), 0, 0) > 0 {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    });
}
