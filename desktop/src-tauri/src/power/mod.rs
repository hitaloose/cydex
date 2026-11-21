//! Eventos de dormir/acordar do computador, um módulo por sistema.

use std::sync::Arc;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PowerEvent {
    /// Tela apagou ou o computador vai dormir.
    Sleep,
    /// Tela acendeu ou o computador acordou.
    Wake,
}

pub type Handler = Arc<dyn Fn(PowerEvent) + Send + Sync + 'static>;

#[cfg(target_os = "macos")]
mod macos;
#[cfg(target_os = "macos")]
pub use macos::start;

#[cfg(windows)]
mod windows;
#[cfg(windows)]
pub use self::windows::start;

#[cfg(target_os = "linux")]
mod linux;
#[cfg(target_os = "linux")]
pub use linux::start;

#[cfg(not(any(target_os = "macos", windows, target_os = "linux")))]
pub fn start(_on: Handler) {}
