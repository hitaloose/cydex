//! Acha o CYDex na rede pelo mDNS: serviço `_http._tcp` com TXT `app=cydex`
//! (firmware novo) ou com a instância chamada `cydex` (firmware antigo).

use mdns_sd::{ServiceDaemon, ServiceEvent};
use serde::Serialize;
use std::time::{Duration, Instant};

#[derive(Serialize, Debug, Clone, PartialEq)]
pub struct Found {
    /// `cydex.local`
    pub host: String,
    pub ip: String,
}

const SERVICE: &str = "_http._tcp.local.";

pub fn browse(wait: Duration) -> Vec<Found> {
    let Ok(mdns) = ServiceDaemon::new() else { return vec![] };
    let Ok(rx) = mdns.browse(SERVICE) else {
        let _ = mdns.shutdown();
        return vec![];
    };
    let deadline = Instant::now() + wait;
    let mut found: Vec<Found> = vec![];
    while let Some(left) = deadline.checked_duration_since(Instant::now()) {
        let Ok(event) = rx.recv_timeout(left) else { break };
        let ServiceEvent::ServiceResolved(info) = event else { continue };
        let instance = info.get_fullname().split('.').next().unwrap_or_default().to_ascii_lowercase();
        let is_cydex = info.get_property_val_str("app") == Some("cydex") || instance == "cydex";
        if !is_cydex {
            continue;
        }
        let host = info.get_hostname().trim_end_matches('.').to_string();
        for ip in info.get_addresses().iter().filter(|ip| ip.is_ipv4()) {
            let f = Found { host: host.clone(), ip: ip.to_string() };
            if !found.contains(&f) {
                found.push(f);
            }
        }
    }
    let _ = mdns.stop_browse(SERVICE);
    let _ = mdns.shutdown();
    found
}
