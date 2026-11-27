//! Acha o IP do CYDex a partir do nome (`cydex.local`).
//!
//! No app empacotado do macOS o mDNS não funciona: nem o getaddrinfo do
//! sistema nem o mdns-sd recebem resposta, mesmo com a permissão de Rede
//! Local (que libera o TCP por IP). No Terminal, as duas coisas resolvem. Por
//! isso tenta, em ordem, e registra no log qual deu certo:
//!
//! 1. mdns-sd (mDNS próprio);
//! 2. resolvedor do sistema (getaddrinfo, só IPv4);
//! 3. macOS: `dscacheutil` como processo separado;
//! 4. varredura da sub-rede: porta 80 respondendo `"name":"CYDex"` no `GET /`.
//!
//! O IP achado fica em cache (`forget` limpa quando a conexão falha).

use std::collections::HashMap;
use std::io;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::sync::{Mutex, OnceLock};
use std::time::{Duration, Instant};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

const TTL: Duration = Duration::from_secs(600);

fn cache() -> &'static Mutex<HashMap<String, (Ipv4Addr, Instant)>> {
    static CACHE: OnceLock<Mutex<HashMap<String, (Ipv4Addr, Instant)>>> = OnceLock::new();
    CACHE.get_or_init(Default::default)
}

fn key(host: &str) -> String {
    host.trim_end_matches('.').to_ascii_lowercase()
}

/// A conexão falhou: o próximo pedido procura de novo (o DHCP pode ter trocado).
pub fn forget(host: &str) {
    cache().lock().unwrap().remove(&key(host));
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
    let k = key(host);
    if let Some((ip, at)) = cache().lock().unwrap().get(&k) {
        if at.elapsed() < TTL {
            return Ok((*ip, "cache"));
        }
    }
    let local = k.ends_with(".local");
    let h = k.clone();
    let found = tokio::task::spawn_blocking(move || {
        if local {
            if let Some(ip) = by_mdns(&h) {
                return Some((ip, "mdns-sd"));
            }
        }
        if let Some(ip) = by_system(&h) {
            return Some((ip, "sistema"));
        }
        #[cfg(target_os = "macos")]
        if local {
            if let Some(ip) = by_dscacheutil(&h) {
                return Some((ip, "dscacheutil"));
            }
        }
        None
    })
    .await
    .ok()
    .flatten();
    let found = match found {
        Some(f) => Some(f),
        // Só pro nome padrão: um nome qualquer não deve virar "o primeiro CYDex".
        None if local => scan(true).await.into_iter().next().map(|ip| (ip, "varredura")),
        None => None,
    };
    match found {
        Some((ip, how)) => {
            log::info!("{k} -> {ip} ({how})");
            cache().lock().unwrap().insert(k, (ip, Instant::now()));
            Ok((ip, how))
        }
        None => {
            log::warn!("{k}: não achei o CYDex");
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
