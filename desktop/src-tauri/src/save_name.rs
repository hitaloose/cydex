//! Nome com que o save vai pro cartão. Mesmas regras do firmware
//! (`lib/core/src/saves/SaveName`): até 64 bytes, ASCII imprimível sem
//! `/ \ : * ? " < > |`, não começa com `.` nem espaço, termina em .sav/.srm.

pub const MAX: usize = 64;
const FORBIDDEN: &str = "/\\:*?\"<>|";

#[derive(Debug, PartialEq, Eq)]
pub enum NameError {
    /// Não é .sav nem .srm: provavelmente não é save.
    Extension,
}

pub fn is_valid(name: &str) -> bool {
    let b = name.as_bytes();
    if b.len() < 5 || b.len() > MAX || b[0] == b'.' || b[0] == b' ' {
        return false;
    }
    if !b.iter().all(|&c| (0x20..=0x7E).contains(&c) && !FORBIDDEN.as_bytes().contains(&c)) {
        return false;
    }
    let ext = name[name.len() - 4..].to_ascii_lowercase();
    ext == ".sav" || ext == ".srm"
}

/// Ajeita o nome do arquivo pro cartão: tira acentos comuns, troca o resto
/// por `_` e encurta mantendo a extensão.
pub fn sanitize(file_name: &str) -> Result<String, NameError> {
    let lower = file_name.to_ascii_lowercase();
    if !lower.ends_with(".sav") && !lower.ends_with(".srm") {
        return Err(NameError::Extension);
    }
    // Os 4 últimos bytes são ASCII (".sav"/".srm"): o corte cai em fronteira de char.
    let ext = &file_name[file_name.len() - 4..];
    let stem = &file_name[..file_name.len() - 4];
    let mut out = String::new();
    for ch in stem.chars() {
        let c = fold(ch);
        if c.is_ascii() && (0x20..=0x7E).contains(&(c as u32)) && !FORBIDDEN.contains(c) {
            out.push(c);
        } else {
            out.push('_');
        }
    }
    let mut out = out.trim_start_matches(['.', ' ']).trim_end().to_string();
    if out.is_empty() {
        out = "save".into();
    }
    if out.len() > MAX - 4 {
        out.truncate(MAX - 4);
        out = out.trim_end().to_string();
    }
    out.push_str(ext);
    debug_assert!(is_valid(&out), "{out}");
    Ok(out)
}

/// Percent-encoding pro caminho da URL (/saves/<nome>).
pub fn url_encode(name: &str) -> String {
    let mut s = String::new();
    for b in name.bytes() {
        if b.is_ascii_alphanumeric() || b"-._~".contains(&b) {
            s.push(b as char);
        } else {
            s.push_str(&format!("%{b:02X}"));
        }
    }
    s
}

fn fold(ch: char) -> char {
    match ch {
        'á' | 'à' | 'â' | 'ã' | 'ä' | 'å' => 'a',
        'Á' | 'À' | 'Â' | 'Ã' | 'Ä' | 'Å' => 'A',
        'é' | 'è' | 'ê' | 'ë' => 'e',
        'É' | 'È' | 'Ê' | 'Ë' => 'E',
        'í' | 'ì' | 'î' | 'ï' => 'i',
        'Í' | 'Ì' | 'Î' | 'Ï' => 'I',
        'ó' | 'ò' | 'ô' | 'õ' | 'ö' => 'o',
        'Ó' | 'Ò' | 'Ô' | 'Õ' | 'Ö' => 'O',
        'ú' | 'ù' | 'û' | 'ü' => 'u',
        'Ú' | 'Ù' | 'Û' | 'Ü' => 'U',
        'ç' => 'c',
        'Ç' => 'C',
        'ñ' => 'n',
        'Ñ' => 'N',
        _ => ch,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn valid_names() {
        assert!(is_valid("emerald.sav"));
        assert!(is_valid("Pokemon - FireRed Version (USA, Europe) (Rev 1).srm"));
        assert!(!is_valid(".hidden.sav"));
        assert!(!is_valid("._x.sav"));
        assert!(!is_valid("a/b.sav"));
        assert!(!is_valid("game.gba"));
        assert!(!is_valid(&format!("{}.sav", "a".repeat(61))));
        assert!(is_valid(&format!("{}.sav", "a".repeat(60))));
    }

    #[test]
    fn sanitize_fixes_what_it_can() {
        assert_eq!(sanitize("emerald.sav").unwrap(), "emerald.sav");
        assert_eq!(sanitize("Pokémon Esmeralda.SAV").unwrap(), "Pokemon Esmeralda.SAV");
        assert_eq!(sanitize("a:b?c.srm").unwrap(), "a_b_c.srm");
        assert_eq!(sanitize("日本.sav").unwrap(), "__.sav");
        assert_eq!(sanitize("..sav").unwrap(), "save.sav");
        let long = sanitize(&format!("{}.sav", "x".repeat(100))).unwrap();
        assert_eq!(long.len(), MAX);
        assert!(is_valid(&long));
        assert_eq!(sanitize("rom.gba"), Err(NameError::Extension));
    }

    #[test]
    fn encodes_url() {
        assert_eq!(url_encode("a b(1).sav"), "a%20b%281%29.sav");
    }
}
