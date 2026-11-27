//! Eventos de dormir/acordar do computador, um módulo por sistema.

use std::sync::Arc;

/// Tela apagou/acendeu ou o computador vai dormir/acordou. O mesmo tipo do
/// registro (`power_log`), que mostra qual dos quatro foi.
pub use crate::power_log::Trigger as PowerEvent;

/// Quem recebe o evento chama `done()` (ou só larga o `Ack`) quando o
/// sistema pode seguir. Só o "vai dormir" do macOS (IOKit) espera por ele: lá
/// o sono fica parado até o `/sleep` sair ou o teto de 5 s passar.
pub struct Ack(Option<Box<dyn FnOnce() + Send>>);

impl Ack {
    /// Evento que não espera nada (tela, acordar, Windows, Linux).
    pub fn none() -> Self {
        Ack(None)
    }

    #[cfg(target_os = "macos")]
    pub fn new(release: impl FnOnce() + Send + 'static) -> Self {
        Ack(Some(Box::new(release)))
    }

    /// Libera o sistema (o mesmo que largar o Ack).
    pub fn done(self) {}

    pub fn waits(&self) -> bool {
        self.0.is_some()
    }
}

impl Drop for Ack {
    fn drop(&mut self) {
        if let Some(release) = self.0.take() {
            release();
        }
    }
}

pub type Handler = Arc<dyn Fn(PowerEvent, Ack) + Send + Sync + 'static>;

#[cfg(target_os = "macos")]
mod macos;
#[cfg(target_os = "macos")]
pub use macos::{begin_activity, start};

/// Fora do macOS não tem App Nap: atividade vazia.
#[cfg(not(target_os = "macos"))]
pub struct Activity;

#[cfg(not(target_os = "macos"))]
pub fn begin_activity(_reason: &str) -> Activity {
    Activity
}

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
