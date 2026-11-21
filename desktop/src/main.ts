import { listen } from "@tauri-apps/api/event";
import { getCurrentWebview } from "@tauri-apps/api/webview";
import { api, type Status } from "./api";
import { el } from "./dom";
import { setLang, t, type Key } from "./i18n";
import { connectionView, dexView, noticesView, onUploadProgress, savesView, type View } from "./views";

type Tab = "dex" | "saves" | "notices" | "connection";

let status: Status = { online: false, info: null, key_ok: null, dex: null, sun: null };
let current: Tab = "dex";
let view: View | null = null;
const saves = savesView();

const views: Record<Tab, () => View> = {
  dex: () => dexView(() => status),
  saves: () => saves,
  notices: () => noticesView(),
  connection: () => connectionView(() => renderAll(), () => status),
};

function renderTabs() {
  const nav = document.getElementById("tabs")!;
  nav.replaceChildren(
    ...(Object.keys(views) as Tab[]).map((tab) => {
      const b = el("button", { role: "tab", "aria-selected": tab === current ? "true" : "false" }, t(`tab.${tab}` as Key));
      b.onclick = () => go(tab);
      return b;
    }),
  );
}

function renderPill() {
  const pill = document.getElementById("status-pill")!;
  let cls = "pill";
  let text: string;
  if (!status.online) text = t("status.offline");
  else if (status.key_ok === null) {
    text = t("status.unpaired");
    cls += " warn";
  } else if (!status.key_ok) {
    text = t("status.bad_key");
    cls += " warn";
  } else {
    text = `${t("status.online")} · ${status.info?.version ?? ""}`;
    cls += " ok";
  }
  pill.className = cls;
  pill.textContent = text;
}

function go(tab: Tab) {
  current = tab;
  renderTabs();
  view = views[tab]();
  const root = document.getElementById("view")!;
  root.replaceChildren();
  view.render(root);
}

function renderAll() {
  renderTabs();
  renderPill();
  go(current);
}

async function main() {
  setLang(await api.getLang());
  status = await api.status();
  const cfg = await api.getConfig();
  // Sem chave ainda: começa pela Conexão.
  current = cfg.key ? "dex" : "connection";
  renderAll();

  await listen<Status>("status", (e) => {
    status = e.payload;
    renderPill();
    view?.onStatus?.(status);
  });
  // A bandeja mudou a configuração (ex.: "Apagar e acender com o computador").
  await listen("config", () => current === "connection" && go("connection"));
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
