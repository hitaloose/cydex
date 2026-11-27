//! Acha o IP do CYDex a partir do nome (`cydex.local`).
//!
//! No app empacotado do macOS o mdns-sd não recebe nada; quem resolve é o
//! getaddrinfo do sistema (e a varredura, se tudo falhar). No Terminal, os
//! dois resolvem. Por isso tenta, em ordem, e registra no log qual deu certo:
//!
//! 1. mdns-sd (mDNS próprio), até falhar uma vez com outro caminho achando:
//!    aí fica de fora até o app fechar (custava 1,8 s em cada procura);
//! 2. resolvedor do sistema (getaddrinfo, só IPv4);
//! 3. macOS: `dscacheutil` como processo separado;
//! 4. varredura da sub-rede: porta 80 respondendo `"name":"CYDex"` no `GET /`.
//!
//! O IP achado fica guardado. Vale 10 min desde a última requisição que deu
//! certo (`touch`); quando uma conexão falha, `forget` só marca como vencido.
//! O último IP conhecido nunca some: os eventos de energia usam ele primeiro,
//! mesmo vencido (`last_known`), e só procuram de novo se ele não responder.

use serde::Serialize;
use std::collections::HashMap;
use std::io;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::{Duration, Instant};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

const TTL: Duration = Duration::from_secs(600);

/// O que se sabe do IP de um nome.
#[derive(Clone, Debug)]
struct Entry {
    ip: Ipv4Addr,
    /// Caminho que achou (`sistema`, `varredura`…).
    how: &'static str,
    found_wall: i64,
    /// Última requisição que deu certo nesse IP.
    ok: Instant,
    ok_wall: Option<i64>,
    /// Uma conexão falhou depois do último ok.
    stale: bool,
}

impl Entry {
    fn new(ip: Ipv4Addr, how: &'static str, now: Instant, wall: i64) -> Self {
        Entry { ip, how, found_wall: wall, ok: now, ok_wall: None, stale: false }
    }

    /// Dá pra usar sem procurar de novo.
    fn fresh(&self, now: Instant) -> bool {
        !self.stale && now.duration_since(self.ok) < TTL
    }

    fn touch(&mut self, now: Instant, wall: i64) {
        self.ok = now;
        self.ok_wall = Some(wall);
        self.stale = false;
    }
}

fn cache() -> &'static Mutex<HashMap<String, Entry>> {
    static CACHE: OnceLock<Mutex<HashMap<String, Entry>>> = OnceLock::new();
    CACHE.get_or_init(Default::default)
}

fn wall_ms() -> i64 {
    chrono::Utc::now().timestamp_millis()
}

fn key(host: &str) -> String {
    host.trim_end_matches('.').to_ascii_lowercase()
}

/// A conexão falhou: o próximo pedido procura de novo (o DHCP pode ter
/// trocado). O IP fica como último conhecido.
pub fn forget(host: &str) {
    if let Some(e) = cache().lock().unwrap().get_mut(&key(host)) {
        e.stale = true;
    }
}

/// Uma requisição pra esse nome deu certo: renova o prazo.
pub fn touch(host: &str) {
    if let Some(e) = cache().lock().unwrap().get_mut(&key(host)) {
        e.touch(Instant::now(), wall_ms());
    }
}

/// Último IP achado pra esse nome, vencido ou não.
pub fn last_known(host: &str) -> Option<(Ipv4Addr, bool)> {
    let now = Instant::now();
    cache().lock().unwrap().get(&key(host)).map(|e| (e.ip, e.fresh(now)))
}

/// Pra tela Conexão ("Como o app acha o CYD").
#[derive(Serialize, Clone, Debug)]
pub struct Known {
    pub ip: String,
    pub how: &'static str,
    /// Quando foi achado e quando deu certo pela última vez (ms desde 1970).
    pub found_at: i64,
    pub ok_at: Option<i64>,
    pub fresh: bool,
}

pub fn known(host: &str) -> Option<Known> {
    let now = Instant::now();
    cache().lock().unwrap().get(&key(host)).map(|e| Known {
        ip: e.ip.to_string(),
        how: e.how,
        found_at: e.found_wall,
        ok_at: e.ok_wall,
        fresh: e.fresh(now),
    })
}

pub async fn resolve(host: &str) -> io::Result<Ipv4Addr> {
    resolve_how(host).await.map(|(ip, _)| ip)
}

/// Como `resolve`, dizendo de onde veio o IP: `ip` (o host já é um IP),
/// `cache` ou o caminho que achou (`mdns-sd`, `sistema`, `dscacheutil`,
/// `varredura`). Vai pro registro de energia.
pub async fn resolve_how(host: &str) -> io::Result<(Ipv4Addr, &'static str)> {
    if let Ok(ip) = host.parse::<Ipv4Addr>() {
        return Ok((ip, "ip"));
    }
    if let Some((ip, true)) = last_known(host) {
        return Ok((ip, "cache"));
    }
    resolve_fresh(host).await
}

/// O mdns-sd já falhou com outro caminho achando o IP: não adianta tentar.
static MDNS_DEAD: AtomicBool = AtomicBool::new(false);

/// Uma procura por vez: no boot várias requisições chegam juntas, e cada
/// uma procurava o mesmo nome (4 getaddrinfo em paralelo no log).
fn searching() -> &'static tokio::sync::Mutex<()> {
    static LOCK: OnceLock<tokio::sync::Mutex<()>> = OnceLock::new();
    LOCK.get_or_init(Default::default)
}

/// Procura de novo, sem olhar o que está guardado (a não ser que outra
/// procura tenha terminado enquanto esta esperava a vez).
pub async fn resolve_fresh(host: &str) -> io::Result<(Ipv4Addr, &'static str)> {
    if let Ok(ip) = host.parse::<Ipv4Addr>() {
        return Ok((ip, "ip"));
    }
    let _one = searching().lock().await;
    if let Some((ip, true)) = last_known(host) {
        return Ok((ip, "cache"));
    }
    let k = key(host);
    let local = k.ends_with(".local");
    let h = k.clone();
    let started = Instant::now();
    let found = tokio::task::spawn_blocking(move || {
        let mut mdns_failed = false;
        if local && !MDNS_DEAD.load(Ordering::Relaxed) {
            if let Some(ip) = by_mdns(&h) {
                return (Some((ip, "mdns-sd")), false);
            }
            mdns_failed = true;
        }
        if let Some(ip) = by_system(&h) {
            return (Some((ip, "sistema")), mdns_failed);
        }
        #[cfg(target_os = "macos")]
        if local {
            if let Some(ip) = by_dscacheutil(&h) {
                return (Some((ip, "dscacheutil")), mdns_failed);
            }
        }
        (None, false)
    })
    .await
    .unwrap_or((None, false));
    let (found, skip_mdns) = found;
    if skip_mdns && !MDNS_DEAD.swap(true, Ordering::Relaxed) {
        log::info!("{k}: mdns-sd não respondeu e o sistema achou; sem mdns-sd até o app fechar");
    }
    let found = match found {
        Some(f) => Some(f),
        // Só pro nome padrão: um nome qualquer não deve virar "o primeiro CYDex".
        None if local => scan(true).await.into_iter().next().map(|ip| (ip, "varredura")),
        None => None,
    };
    match found {
        Some((ip, how)) => {
            log::info!("{k} -> {ip} ({how}, {} ms)", started.elapsed().as_millis());
            cache().lock().unwrap().insert(k, Entry::new(ip, how, Instant::now(), wall_ms()));
            Ok((ip, how))
        }
        None => {
            log::warn!("{k}: não achei o CYDex ({} ms)", started.elapsed().as_millis());
            Err(io::Error::new(io::ErrorKind::NotFound, format!("{k} não encontrado")))
        }
    }
}

fn by_mdns(host: &str) -> Option<Ipv4Addr> {
    use mdns_sd::{HostnameResolutionEvent, ServiceDaemon};
    static DAEMON: OnceLock<Option<ServiceDaemon>> = OnceLock::new();
    let daemon = DAEMON.get_or_init(|| ServiceDaemon::new().ok()).as_ref()?;
    let fqdn = format!("{host}.");
    let rx = daemon.resolve_hostname(&fqdn, Some(1500)).ok()?;
    let deadline = Instant::now() + Duration::from_millis(1800);
    let mut found = None;
    while let Some(left) = deadline.checked_duration_since(Instant::now()) {
        match rx.recv_timeout(left) {
            Ok(HostnameResolutionEvent::AddressesFound(_, addrs)) => {
                found = addrs.iter().find_map(|a| match a {
                    IpAddr::V4(v4) => Some(*v4),
                    _ => None,
                });
                if found.is_some() {
                    break;
                }
            }
            Ok(HostnameResolutionEvent::SearchTimeout(_) | HostnameResolutionEvent::SearchStopped(_)) | Err(_) => break,
            Ok(_) => {}
        }
    }
    let _ = daemon.stop_resolve_hostname(&fqdn);
    found
}

/// Só IPv4: o CYD não tem IPv6, e o macOS espera ~5 s pela resposta AAAA do
/// mDNS antes de desistir (por isso os scripts usam `curl -4`).
fn by_system(host: &str) -> Option<Ipv4Addr> {
    let hints = dns_lookup::AddrInfoHints {
        address: dns_lookup::AddrFamily::Inet.into(),
        socktype: dns_lookup::SockType::Stream.into(),
        ..Default::default()
    };
    match dns_lookup::getaddrinfo(Some(host), None, Some(hints)) {
        Ok(list) => list.filter_map(|a| a.ok()).find_map(|a| match a.sockaddr.ip() {
            IpAddr::V4(v4) => Some(v4),
            _ => None,
        }),
        Err(e) => {
            log::debug!("sistema {host}: {}", io::Error::from(e));
            None
        }
    }
}

#[cfg(target_os = "macos")]
fn by_dscacheutil(host: &str) -> Option<Ipv4Addr> {
    let out = std::process::Command::new("/usr/bin/dscacheutil")
        .args(["-q", "host", "-a", "name", host])
        .output()
        .ok()?;
    String::from_utf8_lossy(&out.stdout)
        .lines()
        .filter_map(|l| l.strip_prefix("ip_address:"))
        .find_map(|ip| ip.trim().parse().ok())
}

/// IPs a varrer: a sub-rede de cada interface IPv4 (no máximo uma /24 por
/// interface, em volta do próprio IP), sem loopback e sem link-local.
fn scan_targets() -> Vec<Ipv4Addr> {
    let Ok(ifaces) = if_addrs::get_if_addrs() else { return vec![] };
    let mut out = vec![];
    for iface in ifaces {
        let if_addrs::IfAddr::V4(v4) = iface.addr else { continue };
        let ip = v4.ip;
        if ip.is_loopback() || ip.is_link_local() || !ip.is_private() {
            continue;
        }
        let mask = u32::from(v4.netmask).max(0xFFFF_FF00);
        let net = u32::from(ip) & mask;
        let bcast = net | !mask;
        for n in (net + 1)..bcast {
            let cand = Ipv4Addr::from(n);
            if cand != ip && !out.contains(&cand) {
                out.push(cand);
            }
        }
    }
    out
}

async fn is_cydex(ip: Ipv4Addr) -> bool {
    let probe = async {
        // Conectar é rápido no LAN; quem não aceita em 700 ms não é o CYD.
        let mut s = tokio::time::timeout(Duration::from_millis(700), TcpStream::connect(SocketAddr::new(IpAddr::V4(ip), 80)))
            .await
            .map_err(|_| io::Error::from(io::ErrorKind::TimedOut))??;
        s.write_all(b"GET / HTTP/1.1\r\nHost: cydex\r\nConnection: close\r\n\r\n").await?;
        // Para assim que aparece a assinatura: o CYD espera o cliente fechar
        // primeiro (ver notice_server), então ler até o fim só daria timeout.
        let mut buf = vec![0u8; 512];
        let mut n = 0;
        while n < buf.len() {
            let r = s.read(&mut buf[n..]).await?;
            if r == 0 {
                break;
            }
            n += r;
            if String::from_utf8_lossy(&buf[..n]).contains("\"name\":\"CYDex\"") {
                return io::Result::Ok(true);
            }
        }
        io::Result::Ok(false)
    };
    matches!(tokio::time::timeout(Duration::from_millis(2000), probe).await, Ok(Ok(true)))
}

/// Conexões abertas ao mesmo tempo na varredura. App aberto pelo Finder tem
/// limite de 256 arquivos: 759 de uma vez falhavam na hora (inclusive a do CYD).
const SCAN_PARALLEL: usize = 64;

/// Varre a sub-rede atrás de CYDex. Só TCP, que funciona onde o mDNS não.
/// `first`: para no primeiro achado.
pub async fn scan(first: bool) -> Vec<Ipv4Addr> {
    let targets = scan_targets();
    let started = Instant::now();
    let sem = std::sync::Arc::new(tokio::sync::Semaphore::new(SCAN_PARALLEL));
    let mut tasks = tokio::task::JoinSet::new();
    for ip in targets.iter().copied() {
        let sem = sem.clone();
        tasks.spawn(async move {
            let _permit = sem.acquire_owned().await.ok()?;
            is_cydex(ip).await.then_some(ip)
        });
    }
    let mut found = vec![];
    while let Some(r) = tasks.join_next().await {
        if let Ok(Some(ip)) = r {
            found.push(ip);
            if first {
                tasks.abort_all();
                break;
            }
        }
    }
    found.sort();
    log::info!("varredura: {} endereços em {:?}, achei {found:?}", targets.len(), started.elapsed());
    found
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn entry_ttl_counts_from_last_ok_and_forget_only_marks_stale() {
        let t0 = Instant::now();
        let ip = Ipv4Addr::new(192, 168, 0, 42);
        let mut e = Entry::new(ip, "sistema", t0, 0);
        assert!(e.fresh(t0 + Duration::from_secs(599)));
        assert!(!e.fresh(t0 + Duration::from_secs(601)));
        // Requisição boa aos 9 min: vale até os 19.
        e.touch(t0 + Duration::from_secs(540), 1);
        assert!(e.fresh(t0 + Duration::from_secs(1100)));
        assert_eq!(e.ok_wall, Some(1));
        e.stale = true;
        assert!(!e.fresh(t0 + Duration::from_secs(541)));
        // Vencido, mas o IP continua lá.
        assert_eq!(e.ip, ip);
        e.touch(t0 + Duration::from_secs(600), 2);
        assert!(e.fresh(t0 + Duration::from_secs(601)));
    }

    #[test]
    fn forget_keeps_last_known() {
        let host = "teste-forget.local";
        cache().lock().unwrap().insert(key(host), Entry::new(Ipv4Addr::new(10, 0, 0, 7), "sistema", Instant::now(), 0));
        assert_eq!(last_known(host), Some((Ipv4Addr::new(10, 0, 0, 7), true)));
        forget(host);
        assert_eq!(last_known(host), Some((Ipv4Addr::new(10, 0, 0, 7), false)));
        touch(host);
        assert_eq!(last_known(host), Some((Ipv4Addr::new(10, 0, 0, 7), true)));
        assert!(known(host).unwrap().ok_at.is_some());
    }

    #[test]
    fn scan_targets_are_private_and_bounded() {
        let t = scan_targets();
        assert!(t.iter().all(|ip| ip.is_private()));
        // No máximo uma /24 por interface.
        assert!(t.len() <= 254 * 8, "{}", t.len());
    }

    /// Contra a rede de verdade: `cargo test -- --ignored live_scan --nocapture`
    #[tokio::test]
    #[ignore]
    async fn live_scan() {
        let found = scan(false).await;
        println!("{found:?}");
        assert!(!found.is_empty());
        let ip = resolve("cydex.local").await.unwrap();
        assert!(found.contains(&ip));
    }
}
