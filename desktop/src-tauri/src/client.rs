//! Cliente da API HTTP do CYDex (ver `src/net/notice_server.h` no firmware).

use crate::dex::{self, DexSummary};
use crate::power_log::Reason;
use crate::save_name;
use reqwest::{Method, StatusCode};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
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

/// `GET /status` (api 4): um indicador do rodapé da Vitrine.
#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct IndicatorInfo {
    pub id: String,
    pub text: String,
    pub age: u64,
}

/// `GET /settings` (api 6): os Ajustes do CYD e as opções que ele aceita.
/// Os nomes são os do `PUT /settings` (firmware: logic/SettingsPatch.h).
#[derive(Deserialize, Serialize, Debug, Clone, PartialEq)]
pub struct CydSettings {
    pub bright: u8,
    pub bright_n: u8,
    pub off_min: u16,
    pub sound: bool,
    pub grid: bool,
    pub sc_int: u16,
    pub sc_rand: bool,
    pub sc_src: u8,
    pub sc_boot: bool,
    pub lang: String,
    pub off_opts: Vec<u16>,
    pub sc_int_opts: Vec<u16>,
    pub langs: Vec<CydLang>,
}

#[derive(Deserialize, Serialize, Debug, Clone, PartialEq)]
pub struct CydLang {
    pub code: String,
    pub name: String,
}

/// Campos que o `PUT /settings` aceita.
pub const SETTING_KEYS: [&str; 10] =
    ["bright", "bright_n", "off_min", "sound", "grid", "sc_int", "sc_rand", "sc_src", "sc_boot", "lang"];

/// `/settings?<campo>=<valor>`, ou None se o campo não existe ou o valor tem
/// caractere fora de `[A-Za-z0-9-]` (o firmware não decodifica %XX aqui e a
/// query tem 47 B).
pub fn setting_path(key: &str, value: &str) -> Option<String> {
    let ok = SETTING_KEYS.contains(&key)
        && !value.is_empty()
        && value.len() <= 11
        && value.chars().all(|c| c.is_ascii_alphanumeric() || c == '-');
    ok.then(|| format!("/settings?{key}={value}"))
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

/// Uma tentativa de `power`: o IP usado (se achou) e o resultado.
pub struct PowerTry {
    pub ip: Option<Ipv4Addr>,
    pub via: Option<&'static str>,
    pub result: std::result::Result<(), Reason>,
}

#[derive(Clone)]
pub struct Api {
    http: reqwest::Client,
    /// Só pro /sleep e /wake, direto no IP: conectar no LAN é rápido, e um
    /// IP morto tem que falhar logo pra dar tempo de procurar de novo.
    power_http: reqwest::Client,
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
        let power_http = reqwest::Client::builder()
            .connect_timeout(POWER_CONNECT)
            .pool_max_idle_per_host(0)
            .user_agent("cydex-desktop")
            .build()
            .expect("cliente HTTP");
        Api { http, power_http }
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
        // O CYD respondeu: o IP guardado vale mais 10 min.
        if let Some(host) = resp.url().host_str() {
            crate::locate::touch(host);
        }
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

    pub async fn indicators(&self, t: &Target<'_>) -> Result<Vec<IndicatorInfo>> {
        self.json(self.request(t, Method::GET, "/status")).await
    }

    pub async fn settings(&self, t: &Target<'_>) -> Result<CydSettings> {
        self.json(self.request(t, Method::GET, "/settings")).await
    }

    /// `PUT /settings?<campo>=<valor>`: um campo por vez.
    pub async fn set_setting(&self, t: &Target<'_>, key: &str, value: &str) -> Result<()> {
        let path = setting_path(key, value).ok_or_else(|| ApiError::Rejected(key.to_string()))?;
        self.send(self.request(t, Method::PUT, &path)).await.map(|_| ())
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

    /// `POST /sleep` ou `/wake` (`route`), uma tentativa. Acha o IP por
    /// fora do reqwest, pra separar no registro a falha de achar o IP da de
    /// conectar. Começa pelo último IP conhecido, mesmo vencido (sem procurar,
    /// que leva segundos); se ele não conectar, procura de novo e tenta o
    /// novo IP. As repetições ficam em quem chama (lib.rs).
    pub async fn power(&self, t: &Target<'_>, route: &str, timeout: Duration) -> PowerTry {
        let mut old = None;
        if let Some((ip, fresh)) = crate::locate::last_known(t.host) {
            let result = self.power_at(ip, t, route, timeout).await;
            let via = if fresh { "cache" } else { "último" };
            match result {
                Err(Reason::Connect) => {
                    log::info!("power: {ip} ({via}) não conectou, procurando de novo");
                    crate::locate::forget(t.host);
                    old = Some(ip);
                }
                _ => return self.finish(t, ip, via, result),
            }
        }
        let (ip, how) = match crate::locate::resolve_fresh(t.host).await {
            Ok(found) => found,
            Err(_) => return PowerTry { ip: old, via: None, result: Err(Reason::Resolve) },
        };
        if old == Some(ip) {
            // Mesmo IP: o CYD está fora do ar, não adianta repetir agora.
            return PowerTry { ip: Some(ip), via: Some(how), result: Err(Reason::Connect) };
        }
        let result = self.power_at(ip, t, route, timeout).await;
        let via = if old.is_some() { RE_VIA.iter().find(|(h, _)| *h == how).map(|(_, v)| *v).unwrap_or("re") } else { how };
        self.finish(t, ip, via, result)
    }

    fn finish(&self, t: &Target<'_>, ip: Ipv4Addr, via: &'static str, result: std::result::Result<(), Reason>) -> PowerTry {
        match result {
            Ok(()) => crate::locate::touch(t.host),
            Err(Reason::Connect) => crate::locate::forget(t.host),
            Err(_) => {}
        }
        PowerTry { ip: Some(ip), via: Some(via), result }
    }

    async fn power_at(&self, ip: Ipv4Addr, t: &Target<'_>, route: &str, timeout: Duration) -> std::result::Result<(), Reason> {
        let mut req = self.power_http.post(format!("http://{ip}{route}")).timeout(timeout);
        if !t.key.is_empty() {
            req = req.bearer_auth(t.key);
        }
        match req.send().await {
            Ok(resp) if resp.status().is_success() => Ok(()),
            Ok(resp) if resp.status() == StatusCode::UNAUTHORIZED => Err(Reason::Unauthorized),
            Ok(resp) => Err(Reason::Http(resp.status().as_u16())),
            // Com o connect_timeout curto, um IP morto também cai em is_connect.
            Err(e) => Err(if e.is_connect() { Reason::Connect } else { Reason::Timeout }),
        }
    }
}

/// Conectar no LAN leva milissegundos; mais que isso é IP morto.
const POWER_CONNECT: Duration = Duration::from_millis(1500);

/// `via` quando o último IP não conectou e foi achado de novo.
const RE_VIA: [(&str, &str); 4] =
    [("mdns-sd", "re-mdns-sd"), ("sistema", "re-sistema"), ("dscacheutil", "re-dscacheutil"), ("varredura", "re-varredura")];

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
    fn setting_path_fits_and_rejects() {
        assert_eq!(setting_path("bright", "70").as_deref(), Some("/settings?bright=70"));
        assert_eq!(setting_path("lang", "pt-BR").as_deref(), Some("/settings?lang=pt-BR"));
        assert_eq!(setting_path("sound", "true").as_deref(), Some("/settings?sound=true"));
        assert_eq!(setting_path("bogus", "1"), None);
        assert_eq!(setting_path("bright", ""), None);
        assert_eq!(setting_path("bright", "1&grid=1"), None);
        assert_eq!(setting_path("lang", "pt BR"), None);
        // Pior caso cabe nos 47 B de query do firmware.
        for k in SETTING_KEYS {
            let p = setting_path(k, "abcdefghijk").unwrap();
            assert!(p.split_once('?').unwrap().1.len() <= 47, "{p}");
        }
    }

    #[test]
    fn settings_parse_firmware_json() {
        let s: CydSettings = serde_json::from_str(
            r#"{"bright":70,"bright_n":30,"off_min":2,"sound":true,"grid":false,"sc_int":60,"sc_rand":false,"sc_src":0,"sc_boot":false,"lang":"pt-BR","off_opts":[1,2,5,10,0],"sc_int_opts":[30,60,120,300,900],"langs":[{"code":"pt-BR","name":"Português (Brasil)"},{"code":"en","name":"English"}]}"#,
        )
        .unwrap();
        assert_eq!((s.bright, s.off_opts.len(), s.langs[1].code.as_str()), (70, 5, "en"));
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
        assert!(api.power(&t, "/wake", Duration::from_secs(3)).await.result.is_ok(), "wake");
    }
}
