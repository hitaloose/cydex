//! CYDex Desktop: app de bandeja que fala com a API HTTP do CYDex.
//!
//! - apaga/acende a tela do CYDex junto com o computador (`power/`);
//! - lista, envia e apaga saves e recarrega o status (`/saves`);
//! - mostra o progresso da Dex (`/dex`) e os avisos abertos (`/notices`);
//! - manda a localidade do brilho de dia/noite (`/location`, `geocode`).

mod client;
mod config;
mod dex;
mod discovery;
#[cfg(target_os = "macos")]
mod dock;
mod geocode;
mod locate;
mod power;
mod power_log;
mod save_name;
mod tray_text;

use client::{Api, ApiError, IndicatorInfo, Info, NoticeInfo, ReloadResult, SaveInfo, SunInfo, Target};
use config::{Config, Location};
use dex::DexSummary;
use power::PowerEvent;
use power_log::{Attempt, Outcome, PowerLog, PowerSnapshot, Reason};
use serde::Serialize;
use serde_json::Value;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;
use tauri::menu::{CheckMenuItem, Menu, MenuItem, PredefinedMenuItem};
use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
use tauri::{AppHandle, Emitter, Manager, State, WindowEvent, Wry};
use tauri_plugin_autostart::MacosLauncher;

/// Saves maiores que isso o firmware recusa (413).
const MAX_SAVE: u64 = 1024 * 1024;
/// Status (online, capturados) na bandeja.
const POLL: Duration = Duration::from_secs(30);
/// Argumento do início com o sistema: abre só na bandeja.
const HIDDEN_ARG: &str = "--hidden";

#[derive(Serialize, Clone, Default, Debug)]
pub struct Status {
    pub online: bool,
    pub info: Option<Info>,
    /// None = sem chave configurada.
    pub key_ok: Option<bool>,
    pub dex: Option<DexSummary>,
    /// Nascer/pôr do sol que o CYD calculou (api 3).
    pub sun: Option<SunInfo>,
}

struct TrayItems {
    status: MenuItem<Wry>,
    /// Linha da tela do CYD (só leitura).
    screen: MenuItem<Wry>,
    sleep: MenuItem<Wry>,
    wake: MenuItem<Wry>,
    sync: CheckMenuItem<Wry>,
    open: MenuItem<Wry>,
    quit: MenuItem<Wry>,
}

struct AppState {
    cfg: Mutex<Config>,
    cfg_path: PathBuf,
    api: Api,
    status: Mutex<Status>,
    /// Cada evento de energia incrementa: a repetição do "acordar" para se
    /// chegar um evento mais novo (dormiu de novo no meio).
    power_seq: AtomicU64,
    /// Últimos envios de energia (tela "Tela e energia").
    power: Mutex<PowerLog>,
    /// Um envio de energia no ar por vez: o CYD atende um cliente por vez, e
    /// dois POSTs juntos (tela apagou + vai dormir) brigavam. Fica preso só
    /// durante cada tentativa, não na espera entre elas.
    power_send: tokio::sync::Mutex<()>,
    tray: Mutex<Option<TrayItems>>,
}

impl AppState {
    fn cfg(&self) -> Config {
        self.cfg.lock().unwrap().clone()
    }
}

fn target(cfg: &Config) -> Target<'_> {
    Target { host: &cfg.host, key: &cfg.key }
}

fn lang_of(cfg: &Config) -> String {
    if !cfg.lang.is_empty() {
        return cfg.lang.clone();
    }
    match sys_locale::get_locale() {
        Some(l) if l.to_ascii_lowercase().starts_with("pt") => "pt-BR".into(),
        _ => "en".into(),
    }
}

// ---------- status ----------

async fn fetch_status(api: &Api, cfg: &Config) -> Status {
    let t = target(cfg);
    let Ok(info) = api.info(&t).await else { return Status::default() };
    let mut st = Status { online: true, info: Some(info.clone()), ..Default::default() };
    if !cfg.is_paired() {
        return st;
    }
    if info.api >= 2 {
        match api.dex(&t).await {
            Ok(d) => {
                st.key_ok = Some(true);
                st.dex = Some(d);
            }
            Err(ApiError::Unauthorized) => st.key_ok = Some(false),
            Err(_) => {}
        }
    } else {
        st.key_ok = Some(api.check_key(&t).await.is_ok());
    }
    if st.key_ok == Some(true) && info.api >= 3 {
        st.sun = sync_location(api, cfg).await;
    }
    st
}

/// Confere a localidade do CYD e reenvia se não bater com a do app: cidade
/// nova, CYD que perdeu a NVS ou fuso que mudou (horário de verão).
async fn sync_location(api: &Api, cfg: &Config) -> Option<SunInfo> {
    let t = target(cfg);
    let mut sun = api.location(&t).await.ok()?;
    let Some(loc) = &cfg.location else { return Some(sun) };
    let Some(tz) = geocode::offset_minutes(&loc.timezone) else { return Some(sun) };
    if !sun.matches(loc.lat, loc.lon, tz) {
        match api.set_location(&t, loc.lat, loc.lon, tz).await {
            Ok(()) => {
                log::info!("localidade enviada: {} ({:.4}, {:.4}, tz {tz})", loc.name, loc.lat, loc.lon);
                sun = api.location(&t).await.ok()?;
            }
            Err(e) => log::warn!("localidade: {e}"),
        }
    }
    Some(sun)
}

async fn refresh(app: &AppHandle) -> Status {
    let state = app.state::<AppState>();
    let cfg = state.cfg();
    let st = fetch_status(&state.api, &cfg).await;
    *state.status.lock().unwrap() = st.clone();
    update_tray(app, &cfg, &st);
    let _ = app.emit("status", &st);
    renew_safety(app, &st).await;
    st
}

fn update_tray(app: &AppHandle, cfg: &Config, st: &Status) {
    let state = app.state::<AppState>();
    let lang = lang_of(cfg);
    let tr = |k| tray_text::get(&lang, k);
    let line = if !st.online {
        tr("offline").to_string()
    } else if st.key_ok == Some(false) {
        tr("bad_key").to_string()
    } else if let Some(d) = &st.dex {
        tr("caught").replace("{n}", &d.caught.to_string()).replace("{t}", &d.count.to_string())
    } else {
        tr("online").to_string()
    };
    if let Some(items) = state.tray.lock().unwrap().as_ref() {
        let _ = items.status.set_text(&line);
        let _ = items.sleep.set_text(tr("sleep_now"));
        let _ = items.wake.set_text(tr("wake_now"));
        let _ = items.sync.set_text(tr("sync"));
        let _ = items.sync.set_checked(cfg.power_sync);
        let _ = items.open.set_text(tr("open"));
        let _ = items.quit.set_text(tr("quit"));
        let _ = items.screen.set_text(screen_line(&lang, &state.power.lock().unwrap().snapshot(cfg.power_sync)));
        let ready = st.online && st.key_ok == Some(true);
        let _ = items.sleep.set_enabled(ready);
        let _ = items.wake.set_enabled(ready);
    }
    if let Some(tray) = app.tray_by_id("main") {
        let _ = tray.set_tooltip(Some(format!("CYDex — {line}")));
    }
}

// ---------- energia ----------

fn now_ms() -> i64 {
    chrono::Utc::now().timestamp_millis()
}

/// "HH:MM" no fuso do computador.
fn clock(ms: i64) -> String {
    use chrono::TimeZone;
    chrono::Local.timestamp_millis_opt(ms).single().map(|t| t.format("%H:%M").to_string()).unwrap_or_default()
}

/// Linha da tela no menu da bandeja.
fn screen_line(lang: &str, snap: &PowerSnapshot) -> String {
    let tr = |k| tray_text::get(lang, k);
    if !snap.sync {
        return tr("screen_off").into();
    }
    match snap.last_sent() {
        None => tr("screen_on").into(),
        Some(a) if a.outcome == Outcome::Failed => tr("screen_failed").replace("{t}", &clock(a.at)),
        Some(a) if a.sleep => tr("screen_slept").replace("{t}", &clock(a.at)),
        Some(a) => tr("screen_ok").replace("{t}", &clock(a.at)),
    }
}

/// Manda o registro pra janela e atualiza a linha da bandeja.
fn emit_power(app: &AppHandle) {
    let state = app.state::<AppState>();
    let cfg = state.cfg();
    let snap = state.power.lock().unwrap().snapshot(cfg.power_sync);
    if let Some(items) = state.tray.lock().unwrap().as_ref() {
        let _ = items.screen.set_text(screen_line(&lang_of(&cfg), &snap));
    }
    let _ = app.emit("power", &snap);
}

/// Prazo da rede de segurança (`/wake?ttl`, api 5): 3 status de 30 s.
const SAFETY_TTL: u32 = 90;

fn wake_route(app: &AppHandle) -> String {
    let api = app.state::<AppState>().status.lock().unwrap().info.as_ref().map_or(0, |i| i.api);
    if api >= 5 {
        format!("/wake?ttl={SAFETY_TTL}")
    } else {
        "/wake".into()
    }
}

/// Status a cada 30 s: renova o prazo enquanto a tela do computador está
/// acesa. Sem renovação, o CYD apaga sozinho em 90 s (computador sumiu sem
/// avisar). Não vai pro registro (seria uma linha a cada 30 s), só pro log
/// em debug. Com o prazo valendo no CYD, só estende (não acende nada).
async fn renew_safety(app: &AppHandle, st: &Status) {
    let state = app.state::<AppState>();
    let cfg = state.cfg();
    let api = st.info.as_ref().map_or(0, |i| i.api);
    let ready = st.online && st.key_ok == Some(true);
    {
        let mut p = state.power.lock().unwrap();
        if ready {
            p.safety = Some(api >= 5);
        }
        if !ready || !p.should_renew(cfg.power_sync) {
            return;
        }
    }
    let route = format!("/wake?ttl={SAFETY_TTL}");
    let r = {
        let _one = state.power_send.lock().await;
        state.api.power(&target(&cfg), &route, Duration::from_secs(3)).await
    };
    match r.result {
        Ok(()) => {
            log::debug!("power: prazo renovado ({SAFETY_TTL} s)");
            state.power.lock().unwrap().renewed(now_ms(), SAFETY_TTL);
        }
        Err(reason) => log::info!("power: renovar o prazo falhou {}", reason.describe()),
    }
    emit_power(app);
}

/// Teto que o sono espera o `/sleep` (macOS, IOKit). Nunca atrasar o sono
/// de verdade: depois disso dorme mesmo sem resposta.
const SLEEP_HOLD: Duration = Duration::from_secs(5);

/// Quantas vezes e com que espera cada envio tenta.
#[derive(Clone, Copy)]
struct Plan {
    tries: u32,
    gap: Duration,
    /// Teto de cada tentativa.
    timeout: Duration,
    /// Teto do envio inteiro (o sono esperando).
    deadline: Option<tokio::time::Instant>,
}

impl Plan {
    fn for_event(event: PowerEvent, holds_sleep: bool) -> Plan {
        let p = |tries, gap_ms, timeout_ms| Plan {
            tries,
            gap: Duration::from_millis(gap_ms),
            timeout: Duration::from_millis(timeout_ms),
            deadline: None,
        };
        match event {
            // O sono está esperando: tentativas curtas dentro dos 5 s.
            PowerEvent::SystemSleep if holds_sleep => Plan { deadline: Some(tokio::time::Instant::now() + SLEEP_HOLD), ..p(3, 300, 2000) },
            // Sem segurar o sono (Windows, Linux): o computador não espera.
            PowerEvent::SystemSleep => p(1, 0, 3000),
            // Só a tela apagou: o computador segue acordado, dá pra repetir.
            PowerEvent::ScreenOff => p(3, 1000, 3000),
            // Acordando: o Wi-Fi e o mDNS levam alguns segundos.
            PowerEvent::ScreenOn | PowerEvent::SystemWake => p(10, 2000, 3000),
            PowerEvent::Manual => p(1, 0, 3000),
        }
    }
}

fn on_power(app: &AppHandle, event: PowerEvent, ack: power::Ack) {
    let state = app.state::<AppState>();
    let cfg = state.cfg();
    if !cfg.power_sync || !cfg.is_paired() {
        log::debug!("power: {event:?} ignorado (sem sincronia ou sem chave)");
        return; // larga o ack: o sono segue na hora
    }
    let sleep = event.is_sleep();
    let now = now_ms();
    state.power.lock().unwrap().want_off = sleep;
    if state.power.lock().unwrap().join(sleep, now) {
        let route = if sleep { "/sleep" } else { "/wake" };
        let a = Attempt { outcome: Outcome::Joined, ..Attempt::new(now, event, sleep, route) };
        log::info!("{}", a.log_line());
        state.power.lock().unwrap().push(a);
        emit_power(app);
        if ack.waits() {
            // Vai dormir logo depois da tela apagar: o sono espera o /sleep
            // que já está no ar terminar (até o teto).
            let app = app.clone();
            tauri::async_runtime::spawn(async move {
                let started = std::time::Instant::now();
                let deadline = tokio::time::Instant::now() + SLEEP_HOLD;
                while app.state::<AppState>().power.lock().unwrap().busy && tokio::time::Instant::now() < deadline {
                    tokio::time::sleep(Duration::from_millis(50)).await;
                }
                log::info!("power: sono liberado em {} ms (esperou o envio anterior)", started.elapsed().as_millis());
                ack.done();
            });
        }
        return;
    }
    let plan = Plan::for_event(event, ack.waits());
    let app = app.clone();
    tauri::async_runtime::spawn(async move {
        let holds = ack.waits();
        run_power(&app, event, sleep, cfg, plan).await;
        if holds {
            log::info!("power: sono liberado");
        }
        ack.done();
    });
}

/// Dispara o envio em segundo plano (bandeja).
fn send_power(app: AppHandle, trigger: PowerEvent, sleep: bool, cfg: Config) {
    tauri::async_runtime::spawn(async move {
        let plan = Plan::for_event(if sleep { PowerEvent::ScreenOff } else { PowerEvent::ScreenOn }, false);
        run_power(&app, trigger, sleep, cfg, plan).await;
    });
}

/// Manda `/sleep` ou `/wake` conforme o `plan` e registra o resultado. Para
/// se chegar um evento mais novo (dormiu de novo no meio) ou o prazo acabar.
async fn run_power(app: &AppHandle, trigger: PowerEvent, sleep: bool, cfg: Config, plan: Plan) -> Attempt {
    let state = app.state::<AppState>();
    // macOS: fora do App Nap até terminar (drop no fim da função).
    let _activity = power::begin_activity("CYDex: apagar/acender a tela junto com o computador");
    let seq = state.power_seq.fetch_add(1, Ordering::SeqCst) + 1;
    let route = if sleep { "/sleep".to_string() } else { wake_route(app) };
    let route = route.as_str();
    let started = std::time::Instant::now();
    let mut a = Attempt::new(now_ms(), trigger, sleep, route);
    if trigger == PowerEvent::Manual {
        state.power.lock().unwrap().want_off = sleep;
    }
    a.held = plan.deadline.is_some();
    state.power.lock().unwrap().busy = true;
    emit_power(app);
    // Tempo que sobra até o prazo (sem prazo: o teto de cada tentativa).
    let left = |cap: Duration| match plan.deadline {
        Some(d) => d.saturating_duration_since(tokio::time::Instant::now()).min(cap),
        None => cap,
    };
    for attempt in 0..plan.tries {
        if state.power_seq.load(Ordering::SeqCst) != seq {
            a.outcome = Outcome::Superseded;
            break;
        }
        if attempt > 0 {
            if left(plan.gap + plan.timeout) <= plan.gap {
                break; // não cabe mais uma tentativa antes do prazo
            }
            tokio::time::sleep(plan.gap).await;
        }
        let budget = left(Duration::MAX);
        let r = tokio::time::timeout(budget, async {
            let _one = state.power_send.lock().await;
            // Esperou o envio anterior terminar: se ele era de um evento mais
            // novo, este já não vale.
            if state.power_seq.load(Ordering::SeqCst) != seq {
                return None;
            }
            Some(state.api.power(&target(&cfg), route, left(plan.timeout)).await)
        })
        .await;
        let r = match r {
            Ok(Some(r)) => {
                a.tries += 1;
                r
            }
            Ok(None) => {
                a.outcome = Outcome::Superseded;
                break;
            }
            Err(_) => {
                // O prazo acabou no meio (procurando o IP ou esperando a vez).
                a.tries += 1;
                a.outcome = Outcome::Failed;
                a.reason = Some(Reason::Timeout);
                break;
            }
        };
        a.ip = r.ip.map(|ip| ip.to_string());
        a.via = r.via.map(String::from);
        match r.result {
            Ok(()) => {
                a.outcome = Outcome::Ok;
                a.reason = None;
                break;
            }
            Err(reason) => {
                a.outcome = Outcome::Failed;
                a.reason = Some(reason);
                // Chave errada ou erro HTTP: repetir não muda nada.
                if matches!(reason, Reason::Unauthorized | Reason::Http(_)) {
                    break;
                }
            }
        }
    }
    a.ms = started.elapsed().as_millis() as u64;
    if a.outcome == Outcome::Failed {
        log::warn!("{}", a.log_line());
    } else {
        log::info!("{}", a.log_line());
    }
    {
        let mut p = state.power.lock().unwrap();
        if a.outcome == Outcome::Ok {
            if sleep {
                p.safety_until = None;
            } else if route.contains("ttl=") {
                p.renewed(now_ms(), SAFETY_TTL);
            }
        }
        p.push(a.clone());
        if state.power_seq.load(Ordering::SeqCst) == seq {
            p.busy = false;
        }
    }
    emit_power(app);
    a
}

// ---------- comandos da interface ----------

type CmdResult<T> = Result<T, ApiError>;

#[tauri::command]
fn get_config(state: State<AppState>) -> Config {
    state.cfg()
}

#[tauri::command]
fn get_lang(state: State<AppState>) -> String {
    lang_of(&state.cfg())
}

#[tauri::command]
async fn save_config(app: AppHandle, cfg: Config) -> CmdResult<Config> {
    let mut cfg = cfg.normalized();
    {
        let state = app.state::<AppState>();
        // A janela não manda a localidade: ela só muda por set/clear_location.
        cfg.location = state.cfg().location;
        config::save(&state.cfg_path, &cfg).map_err(|e| ApiError::Io(e.to_string()))?;
        *state.cfg.lock().unwrap() = cfg.clone();
    }
    emit_power(&app);
    refresh(&app).await;
    Ok(cfg)
}

#[derive(Serialize)]
struct TestResult {
    info: Info,
    key_ok: bool,
}

/// Testa um host/chave sem salvar (tela Conexão).
#[tauri::command]
async fn test_connection(state: State<'_, AppState>, host: String, key: String) -> CmdResult<TestResult> {
    let cfg = Config { host, key, ..Default::default() }.normalized();
    let t = target(&cfg);
    let info = state.api.info(&t).await?;
    let key_ok = !cfg.key.is_empty() && state.api.check_key(&t).await.is_ok();
    Ok(TestResult { info, key_ok })
}

/// mDNS próprio, o nome padrão pelo resolvedor do sistema e, se nada disso
/// achar (app empacotado no macOS), varredura da sub-rede.
#[tauri::command]
async fn discover() -> Vec<discovery::Found> {
    let mut found = tauri::async_runtime::spawn_blocking(|| discovery::browse(Duration::from_secs(3)))
        .await
        .unwrap_or_default();
    if let Ok(ip) = locate::resolve("cydex.local").await {
        let f = discovery::Found { host: "cydex.local".into(), ip: ip.to_string() };
        if !found.iter().any(|x| x.ip == f.ip) {
            found.push(f);
        }
    }
    if found.is_empty() {
        found = locate::scan(false)
            .await
            .into_iter()
            .map(|ip| discovery::Found { host: ip.to_string(), ip: ip.to_string() })
            .collect();
    }
    log::info!("procurar: {found:?}");
    found
}

#[tauri::command]
fn get_status(state: State<AppState>) -> Status {
    state.status.lock().unwrap().clone()
}

#[tauri::command]
async fn refresh_status(app: AppHandle) -> Status {
    refresh(&app).await
}

#[tauri::command]
async fn list_saves(state: State<'_, AppState>) -> CmdResult<Vec<SaveInfo>> {
    let cfg = state.cfg();
    state.api.saves(&target(&cfg)).await
}

#[derive(Serialize)]
struct UploadOutcome {
    file: String,
    /// Nome no cartão (pode ter sido ajeitado).
    name: Option<String>,
    error: Option<ApiError>,
}

#[derive(Serialize)]
struct UploadReport {
    results: Vec<UploadOutcome>,
    reload: Option<ReloadResult>,
}

/// Envia os arquivos e, se algum entrou, recarrega o status do CYDex.
#[tauri::command]
async fn upload_saves(app: AppHandle, paths: Vec<String>) -> CmdResult<UploadReport> {
    let state = app.state::<AppState>();
    let cfg = state.cfg();
    let t = target(&cfg);
    let mut results = vec![];
    let mut any = false;
    for (i, p) in paths.iter().enumerate() {
        let path = PathBuf::from(p);
        let file = path.file_name().map(|f| f.to_string_lossy().into_owned()).unwrap_or_else(|| p.clone());
        let _ = app.emit("upload-progress", (i, paths.len(), &file));
        let outcome = async {
            let name = save_name::sanitize(&file).map_err(|_| ApiError::BadName)?;
            let meta = std::fs::metadata(&path).map_err(|e| ApiError::Io(e.to_string()))?;
            if meta.len() > MAX_SAVE {
                return Err(ApiError::TooLarge);
            }
            let data = std::fs::read(&path).map_err(|e| ApiError::Io(e.to_string()))?;
            state.api.upload(&t, &name, data).await?;
            Ok(name)
        }
        .await;
        match outcome {
            Ok(name) => {
                any = true;
                results.push(UploadOutcome { file, name: Some(name), error: None });
            }
            Err(e) => results.push(UploadOutcome { file, name: None, error: Some(e) }),
        }
    }
    let reload = if any { state.api.reload(&t).await.ok() } else { None };
    if reload.is_some() {
        refresh(&app).await;
    }
    Ok(UploadReport { results, reload })
}

#[tauri::command]
async fn delete_save(state: State<'_, AppState>, name: String) -> CmdResult<()> {
    let cfg = state.cfg();
    state.api.delete_save(&target(&cfg), &name).await
}

#[tauri::command]
async fn reload_saves(app: AppHandle) -> CmdResult<ReloadResult> {
    let r = {
        let state = app.state::<AppState>();
        let cfg = state.cfg();
        state.api.reload(&target(&cfg)).await?
    };
    refresh(&app).await;
    Ok(r)
}

#[tauri::command]
async fn list_notices(state: State<'_, AppState>) -> CmdResult<Vec<NoticeInfo>> {
    let cfg = state.cfg();
    state.api.notices(&target(&cfg)).await
}

#[tauri::command]
async fn send_notice(state: State<'_, AppState>, body: Value) -> CmdResult<String> {
    let cfg = state.cfg();
    state.api.post_notice(&target(&cfg), &body).await
}

/// `id` None = fecha todos.
#[tauri::command]
async fn close_notice(state: State<'_, AppState>, id: Option<String>) -> CmdResult<()> {
    let cfg = state.cfg();
    state.api.close_notice(&target(&cfg), id.as_deref()).await
}

#[tauri::command]
async fn list_indicators(state: State<'_, AppState>) -> CmdResult<Vec<IndicatorInfo>> {
    let cfg = state.cfg();
    state.api.indicators(&target(&cfg)).await
}

/// Arquivo do log (o `tauri-plugin-log` grava em `app_log_dir`).
fn log_file(app: &AppHandle) -> CmdResult<PathBuf> {
    app.path().app_log_dir().map(|d| d.join("CYDex.log")).map_err(|e| ApiError::Io(e.to_string()))
}

#[tauri::command]
fn log_path(app: AppHandle) -> CmdResult<String> {
    let path = log_file(&app)?;
    // Mostra com ~ no lugar da pasta do usuário.
    let shown = match dirs::home_dir().and_then(|h| path.strip_prefix(&h).ok().map(|p| p.to_path_buf())) {
        Some(rel) => format!("~/{}", rel.display()),
        None => path.display().to_string(),
    };
    Ok(shown)
}

/// Abre o log no app padrão ou, com `reveal`, mostra o arquivo na pasta.
#[tauri::command]
fn open_log(app: AppHandle, reveal: bool) -> CmdResult<()> {
    use std::process::Command;
    let path = log_file(&app)?;
    #[cfg(target_os = "macos")]
    let mut cmd = {
        let mut c = Command::new("/usr/bin/open");
        if reveal {
            c.arg("-R");
        }
        c.arg(&path);
        c
    };
    #[cfg(windows)]
    let mut cmd = {
        let mut c = Command::new("explorer");
        if reveal {
            c.arg(format!("/select,{}", path.display()));
        } else {
            c.arg(&path);
        }
        c
    };
    #[cfg(not(any(target_os = "macos", windows)))]
    let mut cmd = {
        let mut c = Command::new("xdg-open");
        c.arg(if reveal { path.parent().unwrap_or(&path) } else { &path });
        c
    };
    cmd.spawn().map(|_| ()).map_err(|e| {
        log::warn!("abrir o log: {e}");
        ApiError::Io(e.to_string())
    })
}

/// Botões "Apagar agora"/"Acender agora" da janela: uma tentativa, e o erro
/// volta pra janela.
#[tauri::command]
async fn power_now(app: AppHandle, sleep: bool) -> CmdResult<()> {
    let cfg = app.state::<AppState>().cfg();
    let a = run_power(&app, PowerEvent::Manual, sleep, cfg, Plan::for_event(PowerEvent::Manual, false)).await;
    match a.reason {
        None => Ok(()),
        Some(Reason::Unauthorized) => Err(ApiError::Unauthorized),
        Some(Reason::Http(s)) => Err(ApiError::Http(s)),
        Some(_) => Err(ApiError::Offline),
    }
}

/// Último IP conhecido do CYD e como foi achado (tela Conexão).
#[tauri::command]
fn get_locate(state: State<AppState>) -> Option<locate::Known> {
    locate::known(&state.cfg().host)
}

#[tauri::command]
fn get_power(state: State<AppState>) -> PowerSnapshot {
    state.power.lock().unwrap().snapshot(state.cfg().power_sync)
}

#[tauri::command]
async fn search_places(state: State<'_, AppState>, query: String) -> CmdResult<Vec<geocode::Place>> {
    let lang = lang_of(&state.cfg());
    geocode::search(&query, &lang).await.map_err(ApiError::Io)
}

fn store_location(app: &AppHandle, location: Option<Location>) -> CmdResult<Config> {
    let state = app.state::<AppState>();
    let mut cfg = state.cfg();
    cfg.location = location;
    config::save(&state.cfg_path, &cfg).map_err(|e| ApiError::Io(e.to_string()))?;
    *state.cfg.lock().unwrap() = cfg.clone();
    Ok(cfg)
}

/// Grava a cidade e manda pro CYD (no próximo status, se estiver fora do ar).
#[tauri::command]
async fn set_location(app: AppHandle, location: Location) -> CmdResult<Status> {
    store_location(&app, Some(location))?;
    Ok(refresh(&app).await)
}

/// Esquece a cidade e tira do CYD (volta ao brilho de dia o tempo todo).
#[tauri::command]
async fn clear_location(app: AppHandle) -> CmdResult<Status> {
    let cfg = store_location(&app, None)?;
    match app.state::<AppState>().api.clear_location(&target(&cfg)).await {
        Ok(()) | Err(ApiError::Offline) => {}
        Err(e) => return Err(e),
    }
    Ok(refresh(&app).await)
}

// ---------- janela e bandeja ----------

/// macOS: com a janela aberta o app fica no Dock como um app normal; com
/// ela fechada, só na bandeja. Windows/Linux já fazem isso sozinhos (janela
/// escondida sai da barra de tarefas).
fn set_in_dock(app: &AppHandle, visible: bool) {
    #[cfg(target_os = "macos")]
    {
        use tauri::ActivationPolicy::{Accessory, Regular};
        let _ = app.set_activation_policy(if visible { Regular } else { Accessory });
        // Fora do .app o Dock mostraria o ícone genérico (a cada troca pra Regular).
        if visible {
            dock::set_icon();
        }
    }
    #[cfg(not(target_os = "macos"))]
    let _ = (app, visible);
}

fn show_window(app: &AppHandle) {
    set_in_dock(app, true);
    if let Some(w) = app.get_webview_window("main") {
        let _ = w.show();
        let _ = w.unminimize();
        let _ = w.set_focus();
    }
}

fn build_tray(app: &AppHandle) -> tauri::Result<()> {
    let state = app.state::<AppState>();
    let cfg = state.cfg();
    let lang = lang_of(&cfg);
    let tr = |k| tray_text::get(&lang, k);
    let items = TrayItems {
        status: MenuItem::with_id(app, "status", tr("online"), false, None::<&str>)?,
        screen: MenuItem::with_id(app, "screen", tr("screen_on"), false, None::<&str>)?,
        sleep: MenuItem::with_id(app, "sleep", tr("sleep_now"), false, None::<&str>)?,
        wake: MenuItem::with_id(app, "wake", tr("wake_now"), false, None::<&str>)?,
        sync: CheckMenuItem::with_id(app, "sync", tr("sync"), true, cfg.power_sync, None::<&str>)?,
        open: MenuItem::with_id(app, "open", tr("open"), true, None::<&str>)?,
        quit: MenuItem::with_id(app, "quit", tr("quit"), true, None::<&str>)?,
    };
    let sep1 = PredefinedMenuItem::separator(app)?;
    let sep2 = PredefinedMenuItem::separator(app)?;
    let menu = Menu::with_items(
        app,
        &[&items.status, &items.screen, &sep1, &items.sleep, &items.wake, &items.sync, &sep2, &items.open, &items.quit],
    )?;

    // macOS: ícone "template" (o sistema pinta conforme o tema da barra).
    #[cfg(target_os = "macos")]
    let icon = tauri::image::Image::from_bytes(include_bytes!("../icons/tray-template.png"))?;
    #[cfg(not(target_os = "macos"))]
    let icon = tauri::image::Image::from_bytes(include_bytes!("../icons/tray.png"))?;

    TrayIconBuilder::with_id("main")
        .icon(icon)
        .icon_as_template(cfg!(target_os = "macos"))
        .tooltip("CYDex")
        .menu(&menu)
        // Windows/Linux: clique esquerdo abre a janela, o direito mostra o menu.
        .show_menu_on_left_click(cfg!(target_os = "macos"))
        .on_menu_event(|app, event| match event.id.as_ref() {
            "open" => show_window(app),
            "quit" => app.exit(0),
            "sleep" | "wake" => {
                let cfg = app.state::<AppState>().cfg();
                let sleep = event.id.as_ref() == "sleep";
                send_power(app.clone(), PowerEvent::Manual, sleep, cfg);
            }
            "sync" => {
                let app = app.clone();
                tauri::async_runtime::spawn(async move {
                    let mut cfg = app.state::<AppState>().cfg();
                    cfg.power_sync = !cfg.power_sync;
                    // save_config avisa a janela (evento "power").
                    let _ = save_config(app.clone(), cfg).await;
                });
            }
            _ => {}
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click { button: MouseButton::Left, button_state: MouseButtonState::Up, .. } = event {
                if !cfg!(target_os = "macos") {
                    show_window(tray.app_handle());
                }
            }
        })
        .build(app)?;
    *state.tray.lock().unwrap() = Some(items);
    Ok(())
}

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_single_instance::init(|app, _args, _cwd| show_window(app)))
        .plugin(tauri_plugin_autostart::init(MacosLauncher::LaunchAgent, Some(vec![HIDDEN_ARG])))
        .plugin(tauri_plugin_dialog::init())
        // Log em arquivo (macOS: ~/Library/Logs/dev.cydex.desktop/CYDex.log).
        .plugin(
            tauri_plugin_log::Builder::new()
                .clear_targets()
                .level(log::LevelFilter::Info)
                .level_for("cydex_desktop_lib", log::LevelFilter::Debug)
                .target(tauri_plugin_log::Target::new(tauri_plugin_log::TargetKind::LogDir { file_name: Some("CYDex".into()) }))
                .target(tauri_plugin_log::Target::new(tauri_plugin_log::TargetKind::Stdout))
                .max_file_size(512 * 1024)
                .build(),
        )
        .setup(|app| {
            let cfg_path = config::file_in(app.path().app_config_dir()?);
            let cfg = config::load(&cfg_path);
            let paired = cfg.is_paired();
            app.manage(AppState {
                cfg: Mutex::new(cfg),
                cfg_path,
                api: Api::new(),
                status: Mutex::new(Status::default()),
                power_seq: AtomicU64::new(0),
                power: Mutex::new(PowerLog::default()),
                power_send: tokio::sync::Mutex::new(()),
                tray: Mutex::new(None),
            });

            // Começa só na bandeja; show_window põe no Dock se a janela abrir.
            set_in_dock(app.handle(), false);

            build_tray(app.handle())?;

            let handle = app.handle().clone();
            power::start(Arc::new(move |e, ack| on_power(&handle, e, ack)));

            let handle = app.handle().clone();
            tauri::async_runtime::spawn(async move {
                loop {
                    refresh(&handle).await;
                    tokio::time::sleep(POLL).await;
                }
            });

            // Aberto pelo usuário (ou sem chave ainda): mostra a janela.
            // Pelo início com o sistema: fica só na bandeja.
            let hidden = std::env::args().any(|a| a == HIDDEN_ARG);
            if !hidden || !paired {
                show_window(app.handle());
            }
            Ok(())
        })
        .on_window_event(|window, event| {
            // Fechar a janela só esconde: sai do Dock e o app segue na bandeja.
            // Sair de vez é pela bandeja ou ⌘Q (ExitRequested, não bloqueado).
            if let WindowEvent::CloseRequested { api, .. } = event {
                api.prevent_close();
                let _ = window.hide();
                set_in_dock(window.app_handle(), false);
            }
        })
        .invoke_handler(tauri::generate_handler![
            get_config,
            get_lang,
            save_config,
            test_connection,
            discover,
            get_status,
            refresh_status,
            list_saves,
            upload_saves,
            delete_save,
            reload_saves,
            list_notices,
            send_notice,
            close_notice,
            list_indicators,
            log_path,
            open_log,
            power_now,
            get_power,
            get_locate,
            search_places,
            set_location,
            clear_location,
        ])
        .build(tauri::generate_context!())
        .expect("erro ao iniciar o CYDex")
        .run(|_app, _event| {
            // Clique no ícone do Dock com a janela escondida: abre de novo.
            #[cfg(target_os = "macos")]
            if let tauri::RunEvent::Reopen { has_visible_windows: false, .. } = _event {
                show_window(_app);
            }
        });
}
