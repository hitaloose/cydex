//! Eventos de dormir/acordar do computador, um módulo por sistema.

use std::sync::Arc;

/// Tela apagou/acendeu ou o computador vai dormir/acordou. O mesmo tipo do
/// registro (`power_log`), que mostra qual dos quatro foi.
pub use crate::power_log::Trigger as PowerEvent;

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
