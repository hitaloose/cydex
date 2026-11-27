import { listen } from "@tauri-apps/api/event";
import { getCurrentWebview } from "@tauri-apps/api/webview";
import { api, type PowerSnapshot, type Status } from "./api";
import { el } from "./dom";
import { setLang, t, type Key } from "./i18n";
import { appView } from "./views/app";
import { signal, type View } from "./views/common";
import { connectionView } from "./views/connection";
import { homeView } from "./views/home";
import { noticesView } from "./views/notices";
import { EMPTY_POWER, powerView, screenLine, screenState, screenSummary, type PowerView } from "./views/power";
import { onUploadProgress, savesView } from "./views/saves";
import { settingsView } from "./views/settings";

// Barra lateral (canvas: DeskNav). A linha separa o que é do CYD do que é do app.
const MAIN = ["home", "saves", "notices", "power", "settings"] as const;
const EXTRA = ["connection", "app"] as const;
type Section = (typeof MAIN)[number] | (typeof EXTRA)[number];

let status: Status = { online: false, info: null, key_ok: null, dex: null, sun: null };
let power: PowerSnapshot = EMPTY_POWER;
let current: Section = "home";
let view: View | null = null;
const saves = savesView();

const views: Record<Section, () => View> = {
  home: () => homeView(() => status, go, () => screenSummary(power)),
  saves: () => saves,
  notices: () => noticesView(() => status),
  power: () => powerView(() => power),
  settings: () => settingsView(() => status),
  connection: () => connectionView(() => status),
  app: () => appView(renderAll),
};

function navButton(s: Section) {
  const b = el(
    "button",
    { class: "nav-item", "aria-current": s === current ? "page" : "false" },
    el("span", { class: "mark" }),
    el("span", { class: "grow" }, t(`nav.${s}` as Key)),
    // Bolinha vermelha: o último envio de energia falhou (some no próximo ok).
    s === "power" && screenState(power) === "failed" ? el("span", { class: "alert", "aria-label": t("nav.power_failed") }) : null,
  );
  b.onclick = () => go(s);
  return b;
}

function renderNav() {
  document.getElementById("nav-items")!.replaceChildren(...MAIN.map(navButton), el("div", { class: "nav-sep" }), ...EXTRA.map(navButton));
}

/// Pé da barra: estado do CYD e da tela. Tocar abre a Conexão.
function renderCard() {
  const card = document.getElementById("cyd-card")!;
  let dot = "";
  let title: string;
  let sub: string;
  if (!status.online) {
    title = t("status.offline");
    sub = t("status.sub_offline");
  } else if (status.key_ok === null) {
    [dot, title, sub] = ["warn", t("status.unpaired"), t("status.sub_unpaired")];
  } else if (!status.key_ok) {
    [dot, title, sub] = ["warn", t("status.bad_key"), t("status.sub_bad_key")];
  } else {
    [dot, title, sub] = ["ok", t("status.online"), t("status.sub_online", { v: status.info?.version ?? "", r: signal(status.info?.rssi) })];
  }
  const [screen, screenCls] = screenLine(power);
  card.replaceChildren(
    el("span", { class: "row" }, el("span", { class: `dot ${dot}` }), el("span", { class: "title" }, title)),
    el("span", { class: "muted tiny" }, sub),
    el("span", { class: `screen-line ${screenCls}` }, screen),
  );
  card.onclick = () => go("connection");
}

function go(s: Section) {
  current = s;
  renderNav();
  view = views[s]();
  const root = document.getElementById("view")!;
  root.replaceChildren();
  view.render(root);
}

function renderAll() {
  renderNav();
  renderCard();
  go(current);
}

async function main() {
  setLang(await api.getLang());
  status = await api.status();
  power = await api.getPower();
  const cfg = await api.getConfig();
  document.getElementById("nav")!.setAttribute("aria-label", t("nav.label"));
  // Sem chave ainda: começa pela Conexão (modo "Ligar ao CYDex").
  current = cfg.key ? "home" : "connection";
  renderAll();

  await listen<Status>("status", (e) => {
    status = e.payload;
    renderCard();
    view?.onStatus?.(status);
  });
  // Registro de energia novo: envio começou/terminou ou a sincronia mudou
  // (pela janela ou pela bandeja).
  await listen<PowerSnapshot>("power", (e) => {
    power = e.payload;
    renderNav();
    renderCard();
    if (current === "power") (view as PowerView).onPower(power);
    else if (current === "home") view?.onStatus?.(status);
  });
  await listen<[number, number, string]>("upload-progress", (e) => onUploadProgress(...e.payload));

  // Soltar arquivos em qualquer lugar da janela envia pro CYDex.
  await getCurrentWebview().onDragDropEvent((e) => {
    const p = e.payload;
    if (p.type === "enter" || p.type === "over") {
      if (current !== "saves") go("saves");
      saves.setDragOver(true);
    } else if (p.type === "leave") {
      saves.setDragOver(false);
    } else if (p.type === "drop") {
      saves.setDragOver(false);
      saves.upload(p.paths);
    }
  });

  // Primeira leitura sem esperar os 30 s do backend.
  api.refresh();
}

main();
