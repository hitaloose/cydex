// Ponte com os comandos do Rust (src-tauri/src/lib.rs).
import { invoke } from "@tauri-apps/api/core";
import { t } from "./i18n";

export interface Location {
  name: string;
  lat: number;
  lon: number;
  timezone: string;
}

export interface Config {
  host: string;
  key: string;
  power_sync: boolean;
  lang: string;
  location?: Location | null;
}

/** GET /location do CYDex (api 3): campos null sem localidade ou sem hora. */
export interface SunInfo {
  lat: number | null;
  lon: number | null;
  tz: number | null;
  night: boolean | null;
  rise: string | null;
  set: string | null;
}

export interface Info {
  name: string;
  version: string;
  api: number;
  rssi: number | null;
}

export interface GenStats {
  first: number;
  last: number;
  seen: number;
  caught: number;
}

export interface DexSummary {
  count: number;
  seen: number;
  caught: number;
  generations: GenStats[];
}

export interface Status {
  online: boolean;
  info: Info | null;
  key_ok: boolean | null;
  dex: DexSummary | null;
  sun: SunInfo | null;
}

export interface SaveInfo {
  name: string;
  size: number;
  mtime: number;
  format: string | null;
}

export interface ReloadResult {
  saves: number;
  parsed: number;
  seen: number;
  caught: number;
}

export interface UploadOutcome {
  file: string;
  name: string | null;
  error: ApiError | null;
}

export interface UploadReport {
  results: UploadOutcome[];
  reload: ReloadResult | null;
}

export interface NoticeInfo {
  id: string;
  tag: string;
  title: string;
  type: string;
  age: number;
}

/** `GET /status` (api 4): indicadores do rodapé da Vitrine. */
export interface IndicatorInfo {
  id: string;
  text: string;
  age: number;
}

export type Trigger = "screen_off" | "system_sleep" | "screen_on" | "system_wake" | "manual";
export type Outcome = "ok" | "failed" | "joined" | "superseded";
export type Reason = { kind: "resolve" | "connect" | "timeout" | "unauthorized" } | { kind: "http"; status: number };

/** Um envio de /sleep ou /wake (src-tauri/src/power_log.rs). */
export interface Attempt {
  at: number;
  trigger: Trigger;
  sleep: boolean;
  route: string;
  outcome: Outcome;
  reason: Reason | null;
  tries: number;
  ms: number;
  ip: string | null;
  via: string | null;
  /** O sono esperou este envio (macOS): `ms` é quanto. */
  held: boolean;
}

export interface PowerSnapshot {
  sync: boolean;
  busy: boolean;
  /** Mais recente primeiro. */
  attempts: Attempt[];
  /** Rede de segurança (api ≥ 5): quando o CYD apaga sozinho, ms. */
  safety_until: number | null;
  safety: boolean | null;
  /** Última renovação do prazo que deu certo, ms. */
  renewed_at: number | null;
}

/** `GET /settings` do CYD (api 6). Nomes iguais aos do `PUT /settings`. */
export interface CydSettings {
  bright: number;
  bright_n: number;
  off_min: number;
  sound: boolean;
  grid: boolean;
  sc_int: number;
  sc_rand: boolean;
  sc_src: number;
  sc_boot: boolean;
  lang: string;
  off_opts: number[];
  sc_int_opts: number[];
  langs: { code: string; name: string }[];
}

/** Último IP conhecido do CYD (src-tauri/src/locate.rs). */
export interface Known {
  ip: string;
  how: string;
  found_at: number;
  ok_at: number | null;
  fresh: boolean;
}

export interface Found {
  host: string;
  ip: string;
}

export interface TestResult {
  info: Info;
  key_ok: boolean;
}

export interface ApiError {
  code: string;
  detail?: string | number;
}

export function errorText(e: unknown): string {
  const err = e as ApiError;
  if (err && typeof err === "object" && "code" in err) {
    const key = `err.${err.code}` as Parameters<typeof t>[0];
    return t(key, { d: err.detail ?? "" });
  }
  return String(e);
}

export const api = {
  getConfig: () => invoke<Config>("get_config"),
  getLang: () => invoke<string>("get_lang"),
  saveConfig: (cfg: Config) => invoke<Config>("save_config", { cfg }),
  test: (host: string, key: string) => invoke<TestResult>("test_connection", { host, key }),
  discover: () => invoke<Found[]>("discover"),
  status: () => invoke<Status>("get_status"),
  refresh: () => invoke<Status>("refresh_status"),
  saves: () => invoke<SaveInfo[]>("list_saves"),
  upload: (paths: string[]) => invoke<UploadReport>("upload_saves", { paths }),
  deleteSave: (name: string) => invoke<void>("delete_save", { name }),
  reload: () => invoke<ReloadResult>("reload_saves"),
  notices: () => invoke<NoticeInfo[]>("list_notices"),
  sendNotice: (body: Record<string, unknown>) => invoke<string>("send_notice", { body }),
  closeNotice: (id: string | null) => invoke<void>("close_notice", { id }),
  indicators: () => invoke<IndicatorInfo[]>("list_indicators"),
  openLog: (reveal: boolean) => invoke<void>("open_log", { reveal }),
  logPath: () => invoke<string>("log_path"),
  power: (sleep: boolean) => invoke<void>("power_now", { sleep }),
  getPower: () => invoke<PowerSnapshot>("get_power"),
  getLocate: () => invoke<Known | null>("get_locate"),
  cydSettings: () => invoke<CydSettings>("get_cyd_settings"),
  setCydSetting: (key: keyof CydSettings, value: string | number | boolean) =>
    invoke<CydSettings>("set_cyd_setting", { key, value: String(value) }),
  searchPlaces: (query: string) => invoke<Location[]>("search_places", { query }),
  setLocation: (location: Location) => invoke<Status>("set_location", { location }),
  clearLocation: () => invoke<Status>("clear_location"),
};
