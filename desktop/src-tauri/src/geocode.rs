//! Busca de cidade pelo nome (localidade do brilho de dia/noite do CYDex).
//!
//! Usa a geocodificação do Open-Meteo (grátis, sem chave). Só roda quando o
//! usuário busca; o CYD não fala com a internet, recebe só lat/lon e o fuso.

use crate::config::Location;
use chrono::{Offset, TimeZone, Utc};
use serde::{Deserialize, Serialize};
use std::time::Duration;

const URL: &str = "https://geocoding-api.open-meteo.com/v1/search";

#[derive(Deserialize, Default)]
struct Response {
    #[serde(default)]
    results: Vec<RawPlace>,
}

#[derive(Deserialize)]
struct RawPlace {
    name: String,
    latitude: f64,
    longitude: f64,
    #[serde(default)]
    timezone: Option<String>,
    #[serde(default)]
    admin1: Option<String>,
    #[serde(default)]
    country: Option<String>,
}

/// Resultado da busca, já no formato que vai pra configuração.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Place {
    #[serde(flatten)]
    pub location: Location,
}

fn parse(text: &str) -> Result<Vec<Place>, serde_json::Error> {
    let resp: Response = serde_json::from_str(text)?;
    Ok(resp
        .results
        .into_iter()
        // Sem fuso não dá pra mostrar o horário no CYD.
        .filter_map(|p| {
            let timezone = p.timezone?;
            timezone.parse::<chrono_tz::Tz>().ok()?;
            let mut parts = vec![p.name];
            for extra in [p.admin1, p.country].into_iter().flatten() {
                if !extra.is_empty() && !parts.contains(&extra) {
                    parts.push(extra);
                }
            }
            Some(Place { location: Location { name: parts.join(", "), lat: p.latitude, lon: p.longitude, timezone } })
        })
        .collect())
}

/// Até 6 cidades com esse nome. `lang` = "pt-BR" ou "en" (nomes traduzidos).
pub async fn search(query: &str, lang: &str) -> Result<Vec<Place>, String> {
    let q = query.trim();
    if q.chars().count() < 2 {
        return Ok(vec![]);
    }
    let language = lang.split('-').next().unwrap_or("en");
    let http = reqwest::Client::builder()
        .timeout(Duration::from_secs(10))
        .user_agent("cydex-desktop")
        .build()
        .map_err(|e| e.to_string())?;
    let resp = http
        .get(URL)
        .query(&[("name", q), ("count", "6"), ("language", language), ("format", "json")])
        .send()
        .await
        .map_err(|e| {
            log::warn!("geocode: {e}");
            e.to_string()
        })?;
    if !resp.status().is_success() {
        return Err(format!("HTTP {}", resp.status().as_u16()));
    }
    let text = resp.text().await.map_err(|e| e.to_string())?;
    parse(&text).map_err(|e| e.to_string())
}

/// Fuso de `timezone` agora, em minutos a leste do UTC (muda no horário de verão).
pub fn offset_minutes(timezone: &str) -> Option<i32> {
    offset_minutes_at(timezone, Utc::now())
}

fn offset_minutes_at(timezone: &str, at: chrono::DateTime<Utc>) -> Option<i32> {
    let tz: chrono_tz::Tz = timezone.parse().ok()?;
    let offset = tz.offset_from_utc_datetime(&at.naive_utc()).fix();
    Some(offset.local_minus_utc() / 60)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_results_and_builds_names() {
        let text = r#"{"results":[
            {"id":3448439,"name":"São Paulo","latitude":-23.5475,"longitude":-46.63611,
             "timezone":"America/Sao_Paulo","country":"Brasil","admin1":"São Paulo"},
            {"id":1,"name":"Lisboa","latitude":38.71667,"longitude":-9.13333,
             "timezone":"Europe/Lisbon","country":"Portugal","admin1":"Lisboa"},
            {"id":2,"name":"Sem fuso","latitude":0,"longitude":0}
        ],"generationtime_ms":0.5}"#;
        let places = parse(text).unwrap();
        assert_eq!(places.len(), 2);
        assert_eq!(places[0].location.name, "São Paulo, Brasil");
        assert_eq!(places[0].location.timezone, "America/Sao_Paulo");
        assert!((places[0].location.lat + 23.5475).abs() < 1e-9);
        assert_eq!(places[1].location.name, "Lisboa, Portugal");
    }

    #[test]
    fn no_results_is_empty() {
        assert!(parse(r#"{"generationtime_ms":0.2}"#).unwrap().is_empty());
    }

    #[test]
    fn offset_follows_dst() {
        let jan = Utc.with_ymd_and_hms(2026, 1, 15, 12, 0, 0).unwrap();
        let jul = Utc.with_ymd_and_hms(2026, 7, 15, 12, 0, 0).unwrap();
        assert_eq!(offset_minutes_at("America/Sao_Paulo", jan), Some(-180));
        assert_eq!(offset_minutes_at("Europe/Lisbon", jan), Some(0));
        assert_eq!(offset_minutes_at("Europe/Lisbon", jul), Some(60));
        assert_eq!(offset_minutes_at("Asia/Kolkata", jul), Some(330));
        assert_eq!(offset_minutes_at("Nowhere/City", jul), None);
    }
}
