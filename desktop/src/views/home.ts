// Início (D01/D01b): progresso da Dex, tela do CYD e atalho pros saves.
import { api, type Status } from "../api";
import { busy, el, svg } from "../dom";
import { t } from "../i18n";
import { header, ROMAN, type View } from "./common";

export type Go = (section: "saves" | "power" | "connection") => void;

function dexCard(st: Status) {
  const d = st.dex!;
  const pct = d.count ? d.caught / d.count : 0;
  const r = 56;
  const len = 2 * Math.PI * r;
  const ring = el(
    "div",
    { class: "ring" },
    svg(
      "svg",
      { viewBox: "0 0 132 132", role: "img", "aria-label": `${Math.round(pct * 100)}% ${t("dex.caught")}` },
      svg("circle", { cx: 66, cy: 66, r, fill: "none", stroke: "var(--surface-2)", "stroke-width": 12 }),
      svg("circle", {
        cx: 66,
        cy: 66,
        r,
        fill: "none",
        stroke: "var(--accent)",
        "stroke-width": 12,
        "stroke-linecap": "round",
        "stroke-dasharray": `${len * pct} ${len}`,
      }),
    ),
    el("div", { class: "center" }, el("div", { class: "pct" }, `${Math.round(pct * 100)}%`), el("div", { class: "muted tiny" }, t("dex.caught"))),
  );
  const gens = el("div", { class: "gens" });
  d.generations.forEach((g, i) => {
    const total = g.last - g.first + 1;
    gens.append(
      el(
        "div",
        { class: "gen", title: t("dex.gen", { g: ROMAN[i] }) + ` (#${g.first}–${g.last})` },
        el("span", { class: "num" }, ROMAN[i]),
        el(
          "div",
          { class: "track" },
          el("div", { class: "seen", style: `width:${(100 * g.seen) / total}%` }),
          el("div", { class: "caught", style: `width:${(100 * g.caught) / total}%` }),
        ),
        el("span", { class: "count" }, `${g.caught} / ${total}`),
      ),
    );
  });
  return el(
    "section",
    { class: "card dex" },
    el("div", { class: "ring-col" }, ring, el("p", { class: "muted tiny center-text" }, t("dex.summary", { c: d.caught, s: d.seen, t: d.count }))),
    el(
      "div",
      { class: "stack tight" },
      gens,
      el("div", { class: "legend" }, el("span", { class: "l-caught" }, t("dex.legend_caught")), el("span", { class: "l-seen" }, t("dex.legend_seen"))),
    ),
  );
}

const ICON_SCREEN = () =>
  svg(
    "svg",
    { width: 16, height: 16, viewBox: "0 0 24 24", fill: "none", stroke: "var(--accent)", "stroke-width": 2.2, "stroke-linecap": "round", "stroke-linejoin": "round", "aria-hidden": "true" },
    svg("rect", { x: 3, y: 4, width: 18, height: 13, rx: 2 }),
    svg("path", { d: "M8 21h8M12 17v4" }),
  );
const ICON_SAVE = () =>
  svg(
    "svg",
    { width: 16, height: 16, viewBox: "0 0 24 24", fill: "none", stroke: "var(--accent)", "stroke-width": 2.2, "stroke-linecap": "round", "stroke-linejoin": "round", "aria-hidden": "true" },
    svg("path", { d: "M5 3h11l3 3v15H5z" }),
    svg("path", { d: "M8 3v5h7V3M8 21v-7h8v7" }),
  );
export const CHEVRON = () =>
  svg(
    "svg",
    { width: 16, height: 16, viewBox: "0 0 24 24", fill: "none", stroke: "var(--muted)", "stroke-width": 2.4, "stroke-linecap": "round", "stroke-linejoin": "round", "aria-hidden": "true" },
    svg("path", { d: "M9 18l6-6-6-6" }),
  );

function linkCard(icon: SVGElement, label: string, title: string, sub: string, onClick: () => void) {
  const b = el(
    "button",
    { class: "card link" },
    el("span", { class: "row" }, icon, el("span", { class: "label grow" }, label), CHEVRON()),
    el("span", { class: "big" }, title),
    el("span", { class: "muted small" }, sub),
  );
  b.onclick = onClick;
  return b;
}

/// `screen` devolve título e linha do card da tela (vem de Tela e energia).
export function homeView(getStatus: () => Status, go: Go, screen: () => [string, string]): View {
  let root: HTMLElement;
  const draw = (st: Status) => {
    const refresh = el("button", { class: "btn ghost" }, t("home.refresh"));
    refresh.onclick = () => busy(refresh, () => api.refresh());
    const parts: HTMLElement[] = [header(t("home.title"), refresh)];
    if (!st.online) {
      const retry = el("button", { class: "btn" }, t("home.retry"));
      retry.onclick = () => busy(retry, () => api.refresh());
      parts.push(
        el(
          "div",
          { class: "banner", role: "status" },
          el("div", { class: "grow" }, el("div", { class: "title" }, t("home.offline_title")), el("div", { class: "muted small" }, t("home.offline_hint"))),
          retry,
        ),
      );
    }
    parts.push(st.dex ? dexCard(st) : el("div", { class: "card empty muted" }, t("home.empty")));
    const [title, sub] = screen();
    parts.push(
      el(
        "div",
        { class: "grid2" },
        linkCard(ICON_SCREEN(), t("home.screen"), title, sub, () => go("power")),
        linkCard(ICON_SAVE(), t("home.saves"), t("home.saves_title"), t("home.saves_hint"), () => go("saves")),
      ),
    );
    root.replaceChildren(...parts);
  };
  return {
    render(r) {
      root = r;
      draw(getStatus());
    },
    onStatus: (st) => root && draw(st),
  };
}
