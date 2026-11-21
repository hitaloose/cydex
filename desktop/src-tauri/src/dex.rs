//! Progresso da Dex a partir do `GET /dex` do CYDex: bits visto/capturado em
//! hex (bit = número − 1, como o `DexStatus` do firmware).

use serde::Serialize;

/// Gerações I–IX do catálogo nacional (as mesmas faixas do firmware).
pub const GENERATIONS: [(u16, u16); 9] = [
    (1, 151),
    (152, 251),
    (252, 386),
    (387, 493),
    (494, 649),
    (650, 721),
    (722, 809),
    (810, 905),
    (906, 1025),
];

#[derive(Serialize, Debug, PartialEq, Clone)]
pub struct GenStats {
    pub first: u16,
    pub last: u16,
    pub seen: u16,
    pub caught: u16,
}

#[derive(Serialize, Debug, PartialEq, Clone)]
pub struct DexSummary {
    pub count: u16,
    pub seen: u16,
    pub caught: u16,
    pub generations: Vec<GenStats>,
}

fn decode_hex(s: &str) -> Option<Vec<u8>> {
    if !s.len().is_multiple_of(2) {
        return None;
    }
    (0..s.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(s.get(i..i + 2)?, 16).ok())
        .collect()
}

fn bit(bits: &[u8], n: u16) -> bool {
    let i = (n - 1) as usize;
    bits.get(i / 8).is_some_and(|b| b & (1 << (i % 8)) != 0)
}

pub fn summarize(count: u16, seen_hex: &str, caught_hex: &str) -> Option<DexSummary> {
    let seen = decode_hex(seen_hex)?;
    let caught = decode_hex(caught_hex)?;
    let mut total_seen = 0;
    let mut total_caught = 0;
    let generations = GENERATIONS
        .iter()
        .filter(|(first, _)| *first <= count)
        .map(|&(first, last)| {
            let last = last.min(count);
            let mut g = GenStats { first, last, seen: 0, caught: 0 };
            for n in first..=last {
                // Capturado implica visto (igual ao firmware).
                let c = bit(&caught, n);
                if c || bit(&seen, n) {
                    g.seen += 1;
                }
                if c {
                    g.caught += 1;
                }
            }
            total_seen += g.seen;
            total_caught += g.caught;
            g
        })
        .collect();
    Some(DexSummary { count, seen: total_seen, caught: total_caught, generations })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn hex_with(ns: &[u16]) -> String {
        let mut b = [0u8; 129];
        for &n in ns {
            let i = (n - 1) as usize;
            b[i / 8] |= 1 << (i % 8);
        }
        b.iter().map(|x| format!("{x:02x}")).collect()
    }

    #[test]
    fn counts_per_generation() {
        let s = summarize(1025, &hex_with(&[1, 151, 152, 1025]), &hex_with(&[1, 1025])).unwrap();
        assert_eq!(s.seen, 4);
        assert_eq!(s.caught, 2);
        assert_eq!(s.generations.len(), 9);
        assert_eq!(s.generations[0], GenStats { first: 1, last: 151, seen: 2, caught: 1 });
        assert_eq!(s.generations[1].seen, 1);
        assert_eq!(s.generations[8], GenStats { first: 906, last: 1025, seen: 1, caught: 1 });
    }

    #[test]
    fn caught_implies_seen() {
        let s = summarize(1025, &hex_with(&[]), &hex_with(&[25])).unwrap();
        assert_eq!((s.seen, s.caught), (1, 1));
    }

    #[test]
    fn rejects_bad_hex() {
        assert!(summarize(1025, "zz", "00").is_none());
        assert!(summarize(1025, "0", "00").is_none());
    }

    #[test]
    fn real_device_sample() {
        // GET /dex do CYDex de teste (FireRed): 182 vistos, 161 capturados.
        let seen = "ffffffffffffffffffffffffffffffffffff3f00410121cd838f211b781481";
        let caught = "ffffffffffffffffffffffffffffffffffff3f00410001040303000a4000";
        let pad = |s: &str| format!("{s}{}", "0".repeat(258 - s.len()));
        let s = summarize(1025, &pad(seen), &pad(caught)).unwrap();
        assert_eq!((s.seen, s.caught), (182, 161));
    }
}
