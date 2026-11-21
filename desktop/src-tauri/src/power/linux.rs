//! Linux: sinais do D-Bus. `PrepareForSleep(bool)` do logind (barramento do
//! sistema) para suspender/retomar e `ActiveChanged(bool)` do protetor de
//! tela (barramento da sessão; freedesktop e GNOME) para tela apagada.

use super::{Handler, PowerEvent};
use futures_util::StreamExt;
use zbus::{message::Type, Connection, MatchRule, MessageStream};

async fn listen(conn: zbus::Result<Connection>, interface: &'static str, member: &'static str, on: Handler) {
    let run = async {
        let conn = conn?;
        let rule = MatchRule::builder()
            .msg_type(Type::Signal)
            .interface(interface)?
            .member(member)?
            .build();
        let mut stream = MessageStream::for_match_rule(rule, &conn, None).await?;
        while let Some(Ok(msg)) = stream.next().await {
            // true = vai dormir / protetor de tela ligou.
            if let Ok(active) = msg.body().deserialize::<bool>() {
                on(if active { PowerEvent::Sleep } else { PowerEvent::Wake });
            }
        }
        zbus::Result::Ok(())
    };
    if let Err(e) = run.await {
        log::info!("power: sem {interface}.{member}: {e}");
    }
}

pub fn start(on: Handler) {
    tauri::async_runtime::spawn(async move {
        tokio::join!(
            listen(Connection::system().await, "org.freedesktop.login1.Manager", "PrepareForSleep", on.clone()),
            listen(Connection::session().await, "org.freedesktop.ScreenSaver", "ActiveChanged", on.clone()),
            listen(Connection::session().await, "org.gnome.ScreenSaver", "ActiveChanged", on.clone()),
        );
    });
}
