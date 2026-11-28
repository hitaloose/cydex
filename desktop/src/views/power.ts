// Tela e energia (D04/D04b): apagar/acender o CYD junto com o computador,
// o estado atual e os últimos envios (src-tauri/src/power_log.rs).
import { api, errorText, type Attempt, type PowerSnapshot, type Reason, type Status } from "../api";
import { busy, el, svg, toast } from "../dom";
import { lang, t, type Key } from "../i18n";
import { fmtAge, header, toggle, type View } from "./common";

export const EMPTY_POWER: PowerSnapshot = { sync: false, busy: false, attempts: [], safety_until: null, safety: null, renewed_at: null };

function clock(ms: number, seconds = false): string {
  return new Date(ms).toLocaleTimeString(lang(), { hour: "2-digit", minute: "2-digit", second: seconds ? "2-digit" : undefined });
}

function duration(ms: number): string {
  if (ms < 1000) return `${ms} ms`;
  return `${(ms / 1000).toLocaleString(lang(), { maximumFractionDigits: 1 })} s`;
}

function lastSent(p: PowerSnapshot): Attempt | undefined {
  return p.attempts.find((a) => a.outcome === "ok" || a.outcome === "failed");
}

function reasonText(r: Reason | null, ip: string | null): string {
  if (!r) return "";
  if (r.kind === "http") return t("power.reason_http", { s: r.status });
  return t(`power.reason_${r.kind}` as Key, { ip: ip ?? "?" });
}

function via(v: string | null): string {
  return v ? t(`via.${v}` as Key) : "";
}

export type ScreenState = "on" | "off" | "slept" | "awake" | "failed";

/// Estado da tela do CYD pelo último envio.
export function screenState(p: PowerSnapshot): ScreenState {
  if (!p.sync) return "off";
  const a = lastSent(p);
  if (!a) return "on";
  if (a.outcome === "failed") return "failed";
  return a.sleep ? "slept" : "awake";
}

/// Rede de segurança (api ≥ 5): quando o CYD apaga sozinho se o app sumir.
function safetyText(p: PowerSnapshot): string {
  if (!p.safety || !p.safety_until || p.safety_until <= Date.now()) return "";
  const age = p.renewed_at ? Math.max(0, Math.round((Date.now() - p.renewed_at) / 1000)) : 0;
  return t("power.safety", { a: fmtAge(age), t: clock(p.safety_until) });
}

/// Título e linha do card "Tela do CYD" do Início.
export function screenSummary(p: PowerSnapshot): [string, string] {
  const a = lastSent(p);
  const safety = safetyText(p);
  const plus = (s: string) => (safety ? `${s} ${safety}` : s);
  switch (screenState(p)) {
    case "off":
      return [t("power.off"), t("power.off_hint")];
    case "on":
      return [t("power.on"), plus(t("power.on_hint"))];
    case "failed":
      return [t("power.failed"), plus(t("power.failed_sub", { e: t(`trigger.${a!.trigger}`), t: clock(a!.at), r: reasonText(a!.reason, a!.ip) }))];
    case "slept":
      return [t("power.slept"), t("power.slept_sub", { e: t(`trigger.${a!.trigger}`), t: clock(a!.at), ms: duration(a!.ms) })];
    case "awake":
      return [t("power.awake"), plus(t("power.awake_sub", { e: t(`trigger.${a!.trigger}`), t: clock(a!.at), ms: duration(a!.ms) }))];
  }
}

function historyRow(a: Attempt) {
  let chip: HTMLElement;
  let detail: string;
  let dot: string;
  if (a.outcome === "ok") {
    chip = el("span", { class: "chip ok" }, t("power.chip_ok", { ms: duration(a.ms) }));
    detail = t("power.detail_ok", { ip: a.ip ?? "?", via: via(a.via), n: a.tries });
    dot = "ok";
  } else if (a.outcome === "failed") {
    chip = el("span", { class: "chip fail" }, t("power.chip_failed"));
    const r = reasonText(a.reason, a.ip);
    detail = `${r.charAt(0).toUpperCase()}${r.slice(1)} · ${t("power.tries", { n: a.tries })}`;
    dot = "fail";
  } else if (a.outcome === "joined") {
    chip = el("span", { class: "chip" }, t("power.chip_joined"));
    detail = t("power.detail_joined");
    dot = "";
  } else {
    chip = el("span", { class: "chip" }, t("power.chip_superseded"));
    detail = t("power.detail_superseded");
    dot = "";
  }
  if (a.held && a.outcome !== "joined") detail += ` · ${t("power.held", { ms: duration(a.ms) })}`;
  return el(
    "div",
    { class: "attempt" },
    el("span", { class: "time" }, clock(a.at, true)),
    el("span", { class: `dot ${dot}` }),
    el("div", { class: "grow" }, el("div", { class: "title ellipsis" }, `${t(`trigger.${a.trigger}`)} → ${a.route}`), el("div", { class: "muted tiny ellipsis" }, detail)),
    chip,
  );
}

const ICON_SCREEN = () =>
  svg(
    "svg",
    { width: 22, height: 22, viewBox: "0 0 24 24", fill: "none", stroke: "currentColor", "stroke-width": 2.2, "stroke-linecap": "round", "stroke-linejoin": "round", "aria-hidden": "true" },
    svg("rect", { x: 3, y: 4, width: 18, height: 13, rx: 2 }),
    svg("path", { d: "M8 21h8M12 17v4" }),
  );

export interface PowerView extends View {
  onPower(p: PowerSnapshot): void;
}

export function powerView(getPower: () => PowerSnapshot, getStatus: () => Status): PowerView {
  let banner: HTMLElement | null = null;
  let state: HTMLElement | null = null;
  let history: HTMLElement | null = null;
  let sync: HTMLButtonElement | null = null;

  const draw = (p: PowerSnapshot) => {
    if (!state || !history || !sync || !banner) return;
    sync.setAttribute("aria-pressed", p.sync ? "true" : "false");
    // D04b: firmware sem /wake?ttl.
    const info = getStatus().info;
    banner.hidden = p.safety !== false || !p.sync;
    banner.replaceChildren(el("div", { class: "small" }, t("power.no_safety", { v: info?.version ?? "?", a: info?.api ?? "?" })));
    const s = screenState(p);
    const [title, sub] = screenSummary(p);
    const buttons = (["power.sleep", "power.wake"] as const).map((k) => {
      const b = el("button", { class: "btn sm" }, t(k));
      b.disabled = p.busy;
      b.onclick = () =>
        busy(b, async () => {
          try {
            await api.power(k === "power.sleep");
          } catch (e) {
            toast(errorText(e), true);
          }
        });
      return b;
    });
    const cls = s === "awake" || s === "on" ? "on" : s === "failed" ? "fail" : s === "slept" ? "muted" : "";
    state.replaceChildren(
      el("div", { class: `icon-box ${cls}` }, ICON_SCREEN()),
      el("div", { class: "grow" }, el("div", { class: "big" }, p.busy ? t("power.busy") : title), el("div", { class: "muted small" }, sub)),
      el("div", { class: "stack tight" }, ...buttons),
    );
    history.replaceChildren(
      el("div", { class: "row space hist-head" }, el("h2", {}, t("power.history")), el("span", { class: "muted tiny" }, t("power.history_hint"))),
      ...(p.attempts.length ? p.attempts.map(historyRow) : [el("p", { class: "muted small pad" }, t("power.history_empty"))]),
    );
  };

  return {
    onPower: draw,
    onStatus: () => draw(getPower()),
    render(root) {
      const openLog = el("button", { class: "btn ghost sm" }, t("power.open_log"));
      openLog.onclick = () => api.openLog(false).catch((e) => toast(String(e), true));
      const p = getPower();
      // Grava pelo save_config, que avisa a janela pelo evento "power".
      sync = toggle(p.sync, t("power.sync"), async (on) => {
        await api.saveConfig({ ...(await api.getConfig()), power_sync: on });
      });
      banner = el("div", { class: "banner warn", role: "note", hidden: true });
      state = el("section", { class: "card now" });
      history = el("section", { class: "card history" });
      root.replaceChildren(
        header(t("power.title"), openLog),
        el(
          "section",
          { class: "card setting" },
          el("div", { class: "grow" }, el("div", { class: "title" }, t("power.sync")), el("div", { class: "muted tiny" }, t("power.sync_hint"))),
          sync,
        ),
        banner,
        state,
        history,
      );
      draw(p);
    },
  };
}
