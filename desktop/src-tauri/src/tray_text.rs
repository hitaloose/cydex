//! Textos do menu da bandeja (a janela tem os dela em `src/i18n.ts`).

pub fn get(lang: &str, key: &str) -> &'static str {
    let pt = lang.to_ascii_lowercase().starts_with("pt");
    match (key, pt) {
        ("online", true) => "CYDex conectado",
        ("online", false) => "CYDex connected",
        ("offline", true) => "CYDex fora da rede",
        ("offline", false) => "CYDex offline",
        ("bad_key", true) => "Chave da API errada",
        ("bad_key", false) => "Wrong API key",
        ("caught", true) => "{n} / {t} capturados",
        ("caught", false) => "{n} / {t} caught",
        ("sleep_now", true) => "Apagar a tela agora",
        ("sleep_now", false) => "Turn screen off now",
        ("wake_now", true) => "Acender a tela agora",
        ("wake_now", false) => "Turn screen on now",
        ("sync", true) => "Apagar e acender com o computador",
        ("sync", false) => "Sleep and wake with this computer",
        ("open", true) => "Abrir o CYDex…",
        ("open", false) => "Open CYDex…",
        ("quit", true) => "Sair",
        ("quit", false) => "Quit",
        _ => "",
    }
}
