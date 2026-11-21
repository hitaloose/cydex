//! Configuração do app: onde está o CYDex e a chave da API.
//!
//! Fica em `<config do app>/config.json`. Na primeira vez, importa o
//! `~/.cydex` dos scripts antigos (`CYDEX_KEY=…`, `CYDEX_HOST=…`), se existir.

use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
#[serde(default)]
pub struct Config {
    /// `cydex.local` ou um IP.
    pub host: String,
    /// Chave da API (8 hex), a que aparece no CYDex em Wi-Fi › ⓘ.
    pub key: String,
    /// Apagar/acender a tela do CYDex junto com o computador.
    pub power_sync: bool,
    /// "" = idioma do sistema; "pt-BR" ou "en".
    pub lang: String,
    /// Localidade do brilho de dia/noite (pôr do sol, `PUT /location`).
    /// Só muda pelos comandos `set_location`/`clear_location`.
    pub location: Option<Location>,
}

/// Cidade escolhida na busca (`geocode`). O CYD recebe lat/lon e o fuso
/// em minutos; o nome IANA fica aqui pra recalcular o fuso no horário de verão.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct Location {
    /// "São Paulo, São Paulo, Brasil".
    pub name: String,
    pub lat: f64,
    pub lon: f64,
    /// "America/Sao_Paulo".
    pub timezone: String,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            host: "cydex.local".into(),
            key: String::new(),
            power_sync: true,
            lang: String::new(),
            location: None,
        }
    }
}

impl Config {
    pub fn is_paired(&self) -> bool {
        !self.key.is_empty()
    }

    /// Tira espaços e barras que vêm de copiar e colar (`http://cydex.local/`).
    pub fn normalized(mut self) -> Self {
        let mut h = self.host.trim().to_string();
        for p in ["http://", "https://"] {
            if let Some(rest) = h.strip_prefix(p) {
                h = rest.to_string();
            }
        }
        self.host = h.trim_end_matches('/').to_string();
        if self.host.is_empty() {
            self.host = Config::default().host;
        }
        self.key = self.key.trim().to_string();
        self
    }
}

pub fn load(path: &Path) -> Config {
    if let Ok(text) = fs::read_to_string(path) {
        if let Ok(cfg) = serde_json::from_str::<Config>(&text) {
            return cfg.normalized();
        }
    }
    let mut cfg = Config::default();
    if let Some(home) = dirs::home_dir() {
        if let Ok(text) = fs::read_to_string(home.join(".cydex")) {
            import_shell_env(&text, &mut cfg);
        }
    }
    cfg.normalized()
}

pub fn save(path: &Path, cfg: &Config) -> std::io::Result<()> {
    if let Some(dir) = path.parent() {
        fs::create_dir_all(dir)?;
    }
    let text = serde_json::to_string_pretty(cfg).expect("config serializa");
    // Grava num temporário e renomeia: não deixa o arquivo pela metade.
    let tmp = path.with_extension("json.tmp");
    fs::write(&tmp, text)?;
    fs::rename(&tmp, path)
}

pub fn file_in(dir: PathBuf) -> PathBuf {
    dir.join("config.json")
}

/// Lê `CHAVE=valor` (com ou sem `export` e aspas), como o `~/.cydex` dos scripts.
fn import_shell_env(text: &str, cfg: &mut Config) {
    for line in text.lines() {
        let line = line.trim();
        let line = line.strip_prefix("export ").unwrap_or(line);
        let Some((k, v)) = line.split_once('=') else { continue };
        let v = v.trim().trim_matches(|c| c == '"' || c == '\'');
        match k.trim() {
            "CYDEX_KEY" => cfg.key = v.to_string(),
            "CYDEX_HOST" if !v.is_empty() => cfg.host = v.to_string(),
            _ => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn imports_dot_cydex() {
        let mut cfg = Config::default();
        import_shell_env(
            "# comentário\nCYDEX_KEY=a1b2c3d4\nexport CYDEX_HOST=\"192.168.0.9\"\nOUTRA=x\n",
            &mut cfg,
        );
        assert_eq!(cfg.key, "a1b2c3d4");
        assert_eq!(cfg.host, "192.168.0.9");
    }

    #[test]
    fn normalizes_pasted_host() {
        let cfg = Config { host: " http://cydex.local/ ".into(), key: " ab ".into(), ..Default::default() };
        let cfg = cfg.normalized();
        assert_eq!(cfg.host, "cydex.local");
        assert_eq!(cfg.key, "ab");
        let empty = Config { host: "".into(), ..Default::default() }.normalized();
        assert_eq!(empty.host, "cydex.local");
    }

    #[test]
    fn save_and_load_roundtrip() {
        let dir = std::env::temp_dir().join(format!("cydex-cfg-{}", std::process::id()));
        let path = file_in(dir.clone());
        let cfg = Config {
            host: "10.0.0.2".into(),
            key: "deadbeef".into(),
            power_sync: false,
            lang: "en".into(),
            location: Some(Location {
                name: "Lisboa, Portugal".into(),
                lat: 38.71667,
                lon: -9.13333,
                timezone: "Europe/Lisbon".into(),
            }),
        };
        save(&path, &cfg).unwrap();
        assert_eq!(load(&path), cfg);
        let _ = fs::remove_dir_all(dir);
    }

    #[test]
    fn old_config_without_location_loads() {
        let cfg: Config = serde_json::from_str(r#"{"host":"cydex.local","key":"ab","power_sync":true,"lang":""}"#).unwrap();
        assert_eq!(cfg.location, None);
        assert_eq!(cfg.key, "ab");
    }
}
