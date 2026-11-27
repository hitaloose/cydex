//! macOS: ícone do Dock fora do `.app`.
//!
//! Empacotado, o macOS tira o ícone do `icon.icns` do bundle. Rodando solto
//! (`npm run tauri dev`, `target/debug/cydex-desktop`) não tem bundle e o
//! Dock mostrava o ícone genérico de executável: aí o app põe o próprio.

use objc2::{AllocAnyThread, MainThreadMarker};
use objc2_app_kit::{NSApplication, NSImage};
use objc2_foundation::NSData;

const ICON: &[u8] = include_bytes!("../icons/icon.png");

/// Dentro de um `.app` o ícone já vem do bundle.
fn bundled() -> bool {
    std::env::current_exe().is_ok_and(|p| p.to_string_lossy().contains(".app/Contents/MacOS/"))
}

/// Chamar na thread principal (o `setup` do Tauri roda nela).
pub fn set_icon() {
    if bundled() {
        return;
    }
    let Some(mtm) = MainThreadMarker::new() else {
        log::warn!("dock: set_icon fora da thread principal");
        return;
    };
    let data = NSData::with_bytes(ICON);
    let Some(image) = NSImage::initWithData(NSImage::alloc(), &data) else {
        log::warn!("dock: não deu pra ler o icon.png");
        return;
    };
    // SAFETY: NSApplication na thread principal, imagem válida.
    unsafe { NSApplication::sharedApplication(mtm).setApplicationIconImage(Some(&image)) };
}
