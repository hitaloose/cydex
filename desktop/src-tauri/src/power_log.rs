//! Registro dos envios de energia (`/sleep`, `/wake`), pra tela "Tela e
//! energia" e a linha da bandeja: o último estado mandado e as últimas
//! tentativas, cada uma com o motivo da falha. Só na memória.

use serde::Serialize;
use std::collections::VecDeque;

/// Quantas tentativas a tela mostra.
pub const KEEP: usize = 8;

/// Eventos na mesma direção mais perto que isso viram um (tela apagou +
/// vai dormir chegam juntos).
pub const JOIN_MS: i64 = 1000;

/// O que pediu o envio.
#[derive(Serialize, Clone, Copy, Debug, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum Trigger {
    /// Tela do computador apagou.
    ScreenOff,
    /// Computador vai dormir.
    SystemSleep,
    /// Tela do computador acendeu.
    ScreenOn,
    /// Computador acordou.
    SystemWake,
    /// Botão da janela ou da bandeja.
    Manual,
}

impl Trigger {
    /// Apagar (os dois de dormir) ou acender.
    pub fn is_sleep(self) -> bool {
        matches!(self, Trigger::ScreenOff | Trigger::SystemSleep)
    }
}

#[derive(Serialize, Clone, Copy, Debug, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum Outcome {
    Ok,
    Failed,
    /// Juntado com um evento igual logo antes (não mandou nada).
    Joined,
    /// Chegou um evento mais novo antes de dar certo.
    Superseded,
}

/// Por que falhou. A ordem é a do caminho: achar o IP, conectar, resposta.
#[derive(Serialize, Clone, Copy, Debug, PartialEq, Eq)]
#[serde(tag = "kind", content = "status", rename_all = "snake_case")]
pub enum Reason {
    /// Não achou o IP do CYD.
    Resolve,
    /// Achou o IP, mas a conexão não abriu.
    Connect,
    /// Conectou, mas a resposta não veio a tempo.
    Timeout,
    /// Chave errada.
    Unauthorized,
    /// Outra resposta de erro.
    Http(u16),
}

impl Reason {
    /// Texto do log em arquivo ("falhou na resolução").
    pub fn describe(&self) -> String {
        match self {
            Reason::Resolve => "na resolução".into(),
            Reason::Connect => "na conexão".into(),
            Reason::Timeout => "esperando a resposta".into(),
            Reason::Unauthorized => "com a chave errada".into(),
            Reason::Http(s) => format!("com HTTP {s}"),
        }
    }
}

#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Attempt {
    /// Quando o evento chegou (ms desde 1970).
    pub at: i64,
    pub trigger: Trigger,
    /// true = /sleep.
    pub sleep: bool,
    /// Rota mandada (`/sleep`, `/wake`, `/wake?ttl=90`).
    pub route: String,
    pub outcome: Outcome,
    pub reason: Option<Reason>,
    /// Tentativas feitas (0 quando juntado).
    pub tries: u32,
    /// Do evento até o fim (ms).
    pub ms: u64,
    /// IP usado e como foi achado (`cache`, `sistema`, `varredura`…).
    pub ip: Option<String>,
    pub via: Option<String>,
    /// O sono esperou este envio (macOS, IOKit): `ms` é quanto ele esperou.
    pub held: bool,
}

impl Attempt {
    pub fn new(at: i64, trigger: Trigger, sleep: bool, route: &str) -> Self {
        Attempt {
            at,
            trigger,
            sleep,
            route: route.into(),
            outcome: Outcome::Failed,
            reason: None,
            tries: 0,
            ms: 0,
            ip: None,
            via: None,
            held: false,
        }
    }

    /// Linha do log em arquivo.
    pub fn log_line(&self) -> String {
        let what = if self.sleep { "sleep" } else { "wake" };
        let ip = match (&self.ip, &self.via) {
            (Some(ip), Some(via)) => format!(", IP {ip} ({via})"),
            (Some(ip), None) => format!(", IP {ip}"),
            _ => String::new(),
        };
        let head = format!("power: {what} ({:?}, {})", self.trigger, self.route);
        let ip = if self.held { format!("{ip}, o sono esperou {} ms", self.ms) } else { ip };
        match self.outcome {
            Outcome::Ok => format!("{head} ok em {} ms, {} tentativa(s){ip}", self.ms, self.tries),
            Outcome::Failed => format!(
                "{head} falhou {} em {} ms, {} tentativa(s){ip}",
                self.reason.map(|r| r.describe()).unwrap_or_default(),
                self.ms,
                self.tries
            ),
            Outcome::Joined => format!("{head} juntado com o anterior"),
            Outcome::Superseded => format!("{head} cancelado por um evento mais novo, {} tentativa(s)", self.tries),
        }
    }
}

/// O que a janela recebe (comando `get_power`, evento `"power"`).
#[derive(Serialize, Clone, Debug, Default)]
pub struct PowerSnapshot {
    /// "Apagar e acender junto com o computador" ligado.
    pub sync: bool,
    /// Envio no ar agora.
    pub busy: bool,
    /// Mais recente primeiro.
    pub attempts: Vec<Attempt>,
    /// Rede de segurança (api ≥ 5): quando o CYD apaga sozinho se ninguém
    /// renovar (ms desde 1970). None = sem prazo.
    pub safety_until: Option<i64>,
    /// O firmware tem a rede de segurança (`/wake?ttl`). None = não sabe ainda.
    pub safety: Option<bool>,
    /// Última renovação do prazo que deu certo (ms desde 1970).
    pub renewed_at: Option<i64>,
}

impl PowerSnapshot {
    /// O último que mandou alguma coisa (ignora juntados e cancelados).
    pub fn last_sent(&self) -> Option<&Attempt> {
        self.attempts.iter().find(|a| matches!(a.outcome, Outcome::Ok | Outcome::Failed))
    }
}

#[derive(Default)]
pub struct PowerLog {
    attempts: VecDeque<Attempt>,
    pub busy: bool,
    /// Último evento aceito: (apagar?, quando).
    last_event: Option<(bool, i64)>,
    pub safety_until: Option<i64>,
    pub safety: Option<bool>,
    pub renewed_at: Option<i64>,
    /// O último pedido (evento ou botão) foi apagar: o status não renova o
    /// prazo, senão acenderia o CYD de novo.
    pub want_off: bool,
}

impl PowerLog {
    pub fn push(&mut self, a: Attempt) {
        self.attempts.push_front(a);
        self.attempts.truncate(KEEP);
    }

    /// Evento do sistema chegando: true = é igual ao anterior em menos de
    /// `JOIN_MS` e não precisa mandar nada. Senão vira o "anterior".
    pub fn join(&mut self, sleep: bool, now: i64) -> bool {
        if let Some((s, at)) = self.last_event {
            if s == sleep && (0..JOIN_MS).contains(&(now - at)) {
                return true;
            }
        }
        self.last_event = Some((sleep, now));
        false
    }

    /// Um `/wake?ttl` deu certo agora (evento ou renovação).
    pub fn renewed(&mut self, now: i64, ttl_s: u32) {
        self.renewed_at = Some(now);
        self.safety_until = Some(now + ttl_s as i64 * 1000);
    }

    /// O status deve renovar o prazo agora?
    pub fn should_renew(&self, sync: bool) -> bool {
        sync && self.safety == Some(true) && !self.want_off && !self.busy
    }

    pub fn snapshot(&self, sync: bool) -> PowerSnapshot {
        PowerSnapshot {
            sync,
            busy: self.busy,
            attempts: self.attempts.iter().cloned().collect(),
            safety_until: self.safety_until,
            safety: self.safety,
            renewed_at: self.renewed_at,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn att(outcome: Outcome) -> Attempt {
        Attempt { outcome, ..Attempt::new(0, Trigger::ScreenOff, true, "/sleep") }
    }

    #[test]
    fn keeps_newest_first_and_bounded() {
        let mut log = PowerLog::default();
        for i in 0..(KEEP as i64 + 3) {
            log.push(Attempt { at: i, ..att(Outcome::Ok) });
        }
        let snap = log.snapshot(true);
        assert_eq!(snap.attempts.len(), KEEP);
        assert_eq!(snap.attempts[0].at, KEEP as i64 + 2);
    }

    #[test]
    fn last_sent_skips_joined_and_superseded() {
        let mut log = PowerLog::default();
        log.push(Attempt { reason: Some(Reason::Connect), ..att(Outcome::Failed) });
        log.push(att(Outcome::Joined));
        log.push(att(Outcome::Superseded));
        assert_eq!(log.snapshot(true).last_sent().unwrap().outcome, Outcome::Failed);
    }

    #[test]
    fn joins_same_direction_within_a_second() {
        let mut log = PowerLog::default();
        assert!(!log.join(true, 10_000)); // tela apagou: manda
        assert!(log.join(true, 10_400)); // vai dormir logo depois: junta
        assert!(log.join(true, 10_999)); // ainda conta do primeiro
        assert!(!log.join(true, 11_000)); // 1 s depois do primeiro: manda
        assert!(!log.join(false, 11_200)); // direção oposta nunca junta
        assert!(log.join(false, 11_300));
        assert!(!log.join(true, 11_350));
        // Relógio voltando (NTP) não junta.
        assert!(!log.join(true, 5_000));
    }

    #[test]
    fn renews_only_when_screen_is_wanted_on() {
        let mut log = PowerLog::default();
        assert!(!log.should_renew(true), "sem saber o api");
        log.safety = Some(true);
        assert!(log.should_renew(true));
        assert!(!log.should_renew(false), "sem sincronia");
        log.want_off = true;
        assert!(!log.should_renew(true), "pediu pra apagar");
        log.want_off = false;
        log.busy = true;
        assert!(!log.should_renew(true), "envio no ar");
        log.busy = false;
        log.renewed(1_000, 90);
        assert_eq!(log.safety_until, Some(91_000));
        assert_eq!(log.snapshot(true).renewed_at, Some(1_000));
    }

    #[test]
    fn log_line_names_the_failure_stage() {
        let a = Attempt { reason: Some(Reason::Resolve), tries: 2, ms: 900, ..att(Outcome::Failed) };
        assert!(a.log_line().contains("falhou na resolução"), "{}", a.log_line());
        let b = Attempt {
            reason: Some(Reason::Connect),
            ip: Some("192.168.0.42".into()),
            via: Some("cache".into()),
            ..att(Outcome::Failed)
        };
        assert!(b.log_line().contains("falhou na conexão") && b.log_line().contains("192.168.0.42 (cache)"));
    }

    #[test]
    fn reason_serializes_for_the_window() {
        assert_eq!(serde_json::to_string(&Reason::Http(500)).unwrap(), r#"{"kind":"http","status":500}"#);
        assert_eq!(serde_json::to_string(&Reason::Resolve).unwrap(), r#"{"kind":"resolve"}"#);
    }
}
