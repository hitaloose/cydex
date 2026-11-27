// Peças comuns das seções (canvas, página "App do computador").
import { errorText, type Status } from "../api";
import { el } from "../dom";
import { t } from "../i18n";

export interface View {
  render(root: HTMLElement): void | Promise<void>;
  /// Status novo do backend (a cada 30 s ou depois de uma ação).
  onStatus?(st: Status): void;
}

export const ROMAN = ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"];

export function fmtAge(s: number): string {
  if (s < 60) return `${s} s`;
  if (s < 3600) return `${Math.floor(s / 60)} min`;
  return `${Math.floor(s / 3600)} h`;
}

export function signal(rssi: number | null | undefined): string {
  if (rssi === null || rssi === undefined) return "—";
  const word = rssi >= -60 ? t("conn.signal_good") : rssi >= -72 ? t("conn.signal_ok") : t("conn.signal_bad");
  return `${word}, ${rssi} dBm`;
}

export function errorBox(e: unknown) {
  return el("div", { class: "msg error" }, errorText(e));
}

/// Cabeçalho da seção: título + botões à direita.
export function header(title: string, ...actions: HTMLElement[]) {
  return el("div", { class: "row space head" }, el("h1", {}, title), el("div", { class: "row" }, ...actions));
}

/// Interruptor (aria-pressed), como os do CYD.
export function toggle(on: boolean, label: string, onChange: (on: boolean) => void | Promise<void>) {
  const b = el("button", { class: "switch", "aria-pressed": on ? "true" : "false", "aria-label": label });
  b.onclick = async () => {
    const next = b.getAttribute("aria-pressed") !== "true";
    b.setAttribute("aria-pressed", next ? "true" : "false");
    b.disabled = true;
    try {
      await onChange(next);
    } catch (e) {
      b.setAttribute("aria-pressed", next ? "false" : "true");
      throw e;
    } finally {
      b.disabled = false;
    }
  };
  return b;
}

/// Segmentado (2–5 opções curtas na linha), como o do CYD.
export function segmented<T>(label: string, options: [T, string][], current: T, onPick: (v: T) => void) {
  const group = el("div", { class: "seg", role: "group", "aria-label": label });
  for (const [v, text] of options) {
    const b = el("button", { "aria-pressed": v === current ? "true" : "false" }, text);
    b.onclick = () => {
      if (b.getAttribute("aria-pressed") === "true") return;
      for (const other of group.querySelectorAll("button")) other.setAttribute("aria-pressed", "false");
      b.setAttribute("aria-pressed", "true");
      onPick(v);
    };
    group.append(b);
  }
  return group;
}

/// Linha de ajuste: título + dica à esquerda, controle à direita.
export function settingRow(title: string, hint: string | null, control: HTMLElement) {
  return el(
    "div",
    { class: "setting" },
    el("div", { class: "grow" }, el("div", { class: "title" }, title), hint ? el("div", { class: "muted small" }, hint) : null),
    control,
  );
}
