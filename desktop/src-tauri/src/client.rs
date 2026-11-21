//! Cliente da API HTTP do CYDex (ver `src/net/notice_server.h` no firmware).

use crate::dex::{self, DexSummary};
use crate::save_name;
use reqwest::{Method, StatusCode};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::net::{IpAddr, SocketAddr};
use std::sync::Arc;
use std::time::Duration;

/// Erros que a interface traduz (o texto vai como código, não como frase).
#[derive(Debug, Serialize, Clone, PartialEq)]
#[serde(tag = "code", content = "detail", rename_all = "snake_case")]
pub enum ApiError {
    /// Não respondeu (desligado, fora da rede, nome não resolve).
    Offline,
    /// Chave errada.
    Unauthorized,
    /// Firmware antigo, sem a rota (api < 2).
    Unsupported,
    /// 400 com o campo recusado.
    Rejected(String),
    TooLarge,
    NoCard,
    Storage,
    BadName,
    Http(u16),
    Io(String),
}

impl std::fmt::Display for ApiError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self:?}")
    }
}

pub type Result<T> = std::result::Result<T, ApiError>;

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct Info {
    pub name: String,
    pub version: String,
    /// Ausente no firmware antigo (só avisos e /sleep /wake).
    #[serde(default = "api_v1")]
    pub api: u32,
    #[serde(default)]
    pub rssi: Option<i32>,
}

fn api_v1() -> u32 {
    1
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct SaveInfo {
    pub name: String,
    pub size: u64,
    pub mtime: u64,
    /// Parser que reconheceu o save (`gen3`); None = não reconhecido.
    pub format: Option<String>,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct ReloadResult {
    pub saves: u32,
    pub parsed: u32,
    pub seen: u32,
    pub caught: u32,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct NoticeInfo {
    pub id: String,
    pub tag: String,
    pub title: String,
    #[serde(rename = "type")]
    pub kind: String,
    pub age: u64,
}

/// `GET /location` (api 3). Sem localidade no CYD, tudo None.
#[derive(Deserialize, Serialize, Debug, Clone, Default, PartialEq)]
pub struct SunInfo {
    #[serde(default)]
    pub lat: Option<f64>,
    #[serde(default)]
    pub lon: Option<f64>,
    /// Fuso (min a leste do UTC) usado pra mostrar os horários.
    #[serde(default)]
    pub tz: Option<i32>,
    /// None = o CYD ainda não tem hora (NTP).
    #[serde(default)]
    pub night: Option<bool>,
    /// "HH:MM" no fuso acima; None sem hora ou no dia/noite polar.
    #[serde(default)]
    pub rise: Option<String>,
    #[serde(default)]
    pub set: Option<String>,
}

impl SunInfo {
    /// O CYD já tem essa localidade? O firmware guarda em float (~1e-6°).
    pub fn matches(&self, lat: f64, lon: f64, tz: i32) -> bool {
        let close = |a: Option<f64>, b: f64| a.is_some_and(|a| (a - b).abs() < 1e-3);
        close(self.lat, lat) && close(self.lon, lon) && self.tz == Some(tz)
    }
}

#[derive(Deserialize)]
struct DexRaw {
    count: u16,
    seen: String,
    caught: String,
}

/// Resolve com `locate` (mDNS, sistema, varredura). Só IPv4: o CYD não tem IPv6.
struct Ipv4Resolver;

impl reqwest::dns::Resolve for Ipv4Resolver {
    fn resolve(&self, name: reqwest::dns::Name) -> reqwest::dns::Resolving {
        let host = name.as_str().to_string();
        Box::pin(async move {
            let ip = crate::locate::resolve(&host)
                .await
                .map_err(|e| Box::new(e) as Box<dyn std::error::Error + Send + Sync>)?;
            Ok(Box::new(std::iter::once(SocketAddr::new(IpAddr::V4(ip), 80))) as reqwest::dns::Addrs)
        })
    }
}

#[derive(Clone)]
pub struct Api {
    http: reqwest::Client,
}

pub struct Target<'a> {
    pub host: &'a str,
    pub key: &'a str,
}

impl Api {
    pub fn new() -> Self {
        let http = reqwest::Client::builder()
            .dns_resolver(Arc::new(Ipv4Resolver))
            // Inclui achar o IP: mDNS (~2 s) e, se falhar, a varredura (~2 s).
            .connect_timeout(Duration::from_secs(8))
            .timeout(Duration::from_secs(10))
            // Um cliente por vez no CYD: sem conexões paradas no pool.
            .pool_max_idle_per_host(0)
            .user_agent("cydex-desktop")
            .build()
            .expect("cliente HTTP");
        Api { http }
    }

    fn request(&self, t: &Target, method: Method, path: &str) -> reqwest::RequestBuilder {
        let req = self.http.request(method, format!("http://{}{}", t.host, path));
        if t.key.is_empty() {
            req
        } else {
            req.bearer_auth(t.key)
        }
    }

    async fn send(&self, req: reqwest::RequestBuilder) -> Result<reqwest::Response> {
        let resp = req.send().await.map_err(|e| {
            // A cadeia inteira: "error sending request" sozinho não diz se foi
            // DNS, conexão recusada ou rede local bloqueada pelo macOS.
            let mut msg = e.to_string();
            let mut src = std::error::Error::source(&e);
            while let Some(s) = src {
                msg.push_str(" ← ");
                msg.push_str(&s.to_string());
                src = s.source();
            }
            log::warn!("{} {}: {msg}", e.url().map(|u| u.path()).unwrap_or("?"), if e.is_timeout() { "timeout" } else { "erro" });
            if let Some(host) = e.url().and_then(|u| u.host_str()) {
                crate::locate::forget(host);
            }
            ApiError::Offline
        })?;
        let status = resp.status();
        log::debug!("{} {}", resp.url().path(), status.as_u16());
        if status.is_success() {
            return Ok(resp);
        }
        Err(match status {
            StatusCode::UNAUTHORIZED => ApiError::Unauthorized,
            StatusCode::NOT_FOUND => ApiError::Unsupported,
            StatusCode::PAYLOAD_TOO_LARGE => ApiError::TooLarge,
            StatusCode::SERVICE_UNAVAILABLE => ApiError::NoCard,
            StatusCode::INSUFFICIENT_STORAGE => ApiError::Storage,
            StatusCode::BAD_REQUEST => {
                let field = resp
                    .json::<Value>()
                    .await
                    .ok()
                    .and_then(|v| v.get("error").and_then(|e| e.as_str()).map(String::from))
                    .unwrap_or_default();
                if field == "name" {
                    ApiError::BadName
                } else {
                    ApiError::Rejected(field)
                }
            }
            s => ApiError::Http(s.as_u16()),
        })
    }

    async fn json<T: for<'de> Deserialize<'de>>(&self, req: reqwest::RequestBuilder) -> Result<T> {
        self.send(req).await?.json::<T>().await.map_err(|_| ApiError::Offline)
    }

    /// `GET /` (sem chave).
    pub async fn info(&self, t: &Target<'_>) -> Result<Info> {
        let req = self.http.get(format!("http://{}/", t.host)).timeout(Duration::from_secs(10));
        let info: Info = self.json(req).await?;
        if info.name != "CYDex" {
            return Err(ApiError::Unsupported);
        }
        Ok(info)
    }

    /// Confere a chave com uma rota que existe desde a api 1.
    pub async fn check_key(&self, t: &Target<'_>) -> Result<()> {
        self.send(self.request(t, Method::GET, "/notices")).await.map(|_| ())
    }

    pub async fn dex(&self, t: &Target<'_>) -> Result<DexSummary> {
        let raw: DexRaw = self.json(self.request(t, Method::GET, "/dex")).await?;
        dex::summarize(raw.count, &raw.seen, &raw.caught).ok_or(ApiError::Http(0))
    }

    pub async fn saves(&self, t: &Target<'_>) -> Result<Vec<SaveInfo>> {
        // Com saves grandes o CYD faz o parse de cada um pra dizer o formato.
        let req = self.request(t, Method::GET, "/saves").timeout(Duration::from_secs(20));
        self.json(req).await
    }

    /// `PUT /saves/<nome>`. Idempotente: repete se a rede falhar no meio
    /// (o CYD grava num temporário e só renomeia no fim).
    pub async fn upload(&self, t: &Target<'_>, name: &str, data: Vec<u8>) -> Result<()> {
        let path = format!("/saves/{}", save_name::url_encode(name));
        let mut last = ApiError::Offline;
        for attempt in 0..3 {
            if attempt > 0 {
                tokio::time::sleep(Duration::from_secs(2)).await;
            }
            let req = self
                .request(t, Method::PUT, &path)
                .header("Content-Type", "application/octet-stream")
                .timeout(Duration::from_secs(30))
                .body(data.clone());
            match self.send(req).await {
                Ok(_) => return Ok(()),
                Err(ApiError::Offline) => last = ApiError::Offline,
                Err(e) => return Err(e),
            }
        }
        Err(last)
    }

    pub async fn delete_save(&self, t: &Target<'_>, name: &str) -> Result<()> {
        let path = format!("/saves/{}", save_name::url_encode(name));
        self.send(self.request(t, Method::DELETE, &path)).await.map(|_| ())
    }

    pub async fn reload(&self, t: &Target<'_>) -> Result<ReloadResult> {
        let req = self.request(t, Method::POST, "/saves/reload").timeout(Duration::from_secs(30));
        self.json(req).await
    }

    pub async fn notices(&self, t: &Target<'_>) -> Result<Vec<NoticeInfo>> {
        self.json(self.request(t, Method::GET, "/notices")).await
    }

    pub async fn post_notice(&self, t: &Target<'_>, body: &Value) -> Result<String> {
        let v: Value = self.json(self.request(t, Method::POST, "/notices").json(body)).await?;
        Ok(v.get("id").and_then(|x| x.as_str()).unwrap_or_default().to_string())
    }

    /// `id` None = fecha todos.
    pub async fn close_notice(&self, t: &Target<'_>, id: Option<&str>) -> Result<()> {
        let path = match id {
            Some(id) => format!("/notices/{}", save_name::url_encode(id)),
            None => "/notices".into(),
        };
        self.send(self.request(t, Method::DELETE, &path)).await.map(|_| ())
    }

    pub async fn location(&self, t: &Target<'_>) -> Result<SunInfo> {
        self.json(self.request(t, Method::GET, "/location")).await
    }

    /// `PUT /location?lat=…&lon=…&tz=…` (sem corpo). 4 casas (~11 m) cabem na
    /// query de 47 B do firmware.
    pub async fn set_location(&self, t: &Target<'_>, lat: f64, lon: f64, tz_min: i32) -> Result<()> {
        let path = location_path(lat, lon, tz_min);
        self.send(self.request(t, Method::PUT, &path)).await.map(|_| ())
    }

    pub async fn clear_location(&self, t: &Target<'_>) -> Result<()> {
        self.send(self.request(t, Method::DELETE, "/location")).await.map(|_| ())
    }

    /// `POST /sleep` ou `/wake`, uma tentativa curta. As repetições ficam em
    /// quem chama (lib.rs), que desiste se chegar um evento mais novo.
    pub async fn power(&self, t: &Target<'_>, sleep: bool) -> Result<()> {
        let path = if sleep { "/sleep" } else { "/wake" };
        let req = self.request(t, Method::POST, path).timeout(Duration::from_secs(3));
        self.send(req).await.map(|_| ())
    }
}

fn location_path(lat: f64, lon: f64, tz_min: i32) -> String {
    format!("/location?lat={lat:.4}&lon={lon:.4}&tz={tz_min}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn location_query_fits_firmware_buffer() {
        // Pior caso: sinal e 3 dígitos em tudo. O firmware guarda 47 B de query.
        let path = location_path(-89.99999, -179.99999, -720);
        let query = path.split_once('?').unwrap().1;
        assert_eq!(query, "lat=-90.0000&lon=-180.0000&tz=-720");
        assert!(query.len() <= 47);
    }

    #[test]
    fn sun_info_matches_with_float_rounding() {
        let info: SunInfo =
            serde_json::from_str(r#"{"lat":-23.5475,"lon":-46.6361,"tz":-180,"night":false,"rise":"06:01","set":null}"#)
                .unwrap();
        assert!(info.matches(-23.54750001, -46.63611, -180));
        assert!(!info.matches(-23.5475, -46.6361, -120));
        assert!(!SunInfo::default().matches(0.0, 0.0, 0));
        let empty: SunInfo = serde_json::from_str("{}").unwrap();
        assert_eq!(empty, SunInfo::default());
    }
}

/// Contra um CYDex de verdade (chave do ~/.cydex):
/// `cargo test -- --ignored live_device --nocapture`
#[cfg(test)]
mod live {
    use super::*;

    #[tokio::test]
    #[ignore]
    async fn live_device() {
        let cfg = crate::config::load(std::path::Path::new("/nonexistent"));
        assert!(cfg.is_paired(), "sem CYDEX_KEY no ~/.cydex");
        let t = Target { host: &cfg.host, key: &cfg.key };
        let api = Api::new();
        let start = std::time::Instant::now();
        let info = api.info(&t).await.expect("info");
        println!("info {info:?} em {:?}", start.elapsed());
        assert!(info.api >= 2);
        api.check_key(&t).await.expect("chave");
        let bad = Target { host: &cfg.host, key: "00000000" };
        assert_eq!(api.check_key(&bad).await, Err(ApiError::Unauthorized));
        let dex = api.dex(&t).await.expect("dex");
        println!("dex {} vistos, {} capturados", dex.seen, dex.caught);
        let saves = api.saves(&t).await.expect("saves");
        println!("saves {saves:?}");

        // Upload de um save pequeno (não reconhecido), recarrega e apaga.
        let name = "cydex desktop teste.sav";
        api.upload(&t, name, vec![0u8; 8192]).await.expect("upload");
        let saves = api.saves(&t).await.unwrap();
        let up = saves.iter().find(|s| s.name == name).expect("apareceu na lista");
        assert_eq!((up.size, up.format.clone()), (8192, None));
        assert_eq!(api.upload(&t, "x.gba", vec![1]).await, Err(ApiError::BadName));
        let r = api.reload(&t).await.expect("reload");
        println!("reload {r:?}");
        api.delete_save(&t, name).await.expect("delete");
        assert!(!api.saves(&t).await.unwrap().iter().any(|s| s.name == name));
        api.reload(&t).await.unwrap();

        let id = api
            .post_notice(&t, &serde_json::json!({"title": "CYDex Desktop", "type": "success", "sound": false, "timeout": 10, "id": "desktop-test"}))
            .await
            .expect("aviso");
        assert_eq!(id, "desktop-test");
        assert!(api.notices(&t).await.unwrap().iter().any(|n| n.id == id));
        api.close_notice(&t, Some(&id)).await.unwrap();
        assert!(!api.notices(&t).await.unwrap().iter().any(|n| n.id == id));
        api.power(&t, false).await.expect("wake");
    }
}
