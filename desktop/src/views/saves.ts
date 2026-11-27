// Saves (D02/D02b): soltar ou escolher arquivos, lista do cartão, apagar.
import { ask, open } from "@tauri-apps/plugin-dialog";
import { api, errorText, type SaveInfo, type UploadReport } from "../api";
import { busy, el, toast } from "../dom";
import { lang, t } from "../i18n";
import { errorBox, header, type View } from "./common";

export interface SavesView extends View {
  upload(paths: string[]): Promise<void>;
  setDragOver(over: boolean): void;
}

export function savesView(): SavesView {
  let list: HTMLElement;
  let drop: HTMLElement;
  let dropText: HTMLElement;
  let report: HTMLElement;
  let uploading = false;

  const reloaded = (r: { parsed: number; saves: number; caught: number; seen: number }) =>
    t("saves.reloaded", { p: r.parsed, s: r.saves, c: r.caught, v: r.seen });

  const loadList = async () => {
    list.replaceChildren(el("p", { class: "muted" }, "…"));
    let saves: SaveInfo[];
    try {
      saves = await api.saves();
    } catch (e) {
      list.replaceChildren(errorBox(e));
      return;
    }
    if (!saves.length) {
      list.replaceChildren(el("p", { class: "muted pad" }, t("saves.empty")));
      return;
    }
    const body = el("tbody");
    for (const s of saves) {
      const del = el("button", { class: "btn ghost danger sm" }, t("saves.delete"));
      del.onclick = async () => {
        if (!(await ask(t("saves.delete_ask", { n: s.name }), { kind: "warning", title: "CYDex" }))) return;
        await busy(del, async () => {
          try {
            await api.deleteSave(s.name);
            toast(reloaded(await api.reload()));
          } catch (e) {
            toast(errorText(e), true);
          }
        });
        loadList();
      };
      // Sem NTP (firmware antigo, ou cartão gravado no PC) a data pode vir de 1980.
      const date = s.mtime > 1577836800 ? new Date(s.mtime * 1000).toLocaleDateString(lang()) : "—";
      body.append(
        el(
          "tr",
          {},
          el("td", { class: "name" }, s.name),
          el("td", {}, s.format ? el("span", { class: "chip ok" }, s.format) : el("span", { class: "chip" }, t("saves.unknown"))),
          el("td", { class: "num" }, t("saves.size_kb", { n: Math.round(s.size / 1024) })),
          el("td", { class: "num" }, date),
          el("td", { class: "act" }, del),
        ),
      );
    }
    const head = el(
      "thead",
      {},
      el(
        "tr",
        {},
        el("th", {}, t("saves.col_file")),
        el("th", {}, t("saves.col_format")),
        el("th", { class: "num" }, t("saves.col_size")),
        el("th", { class: "num" }, t("saves.col_date")),
        el("th", {}),
      ),
    );
    list.replaceChildren(el("table", {}, head, body));
  };

  const showReport = (r: UploadReport) => {
    const lines: HTMLElement[] = [];
    const ok = r.results.filter((x) => x.name);
    if (ok.length) lines.push(el("div", { class: "msg ok" }, t("saves.sent", { n: ok.length })));
    for (const x of ok) {
      if (x.name !== x.file) lines.push(el("div", { class: "msg" }, t("saves.renamed", { f: x.file, n: x.name! })));
    }
    for (const x of r.results.filter((x) => x.error)) {
      lines.push(el("div", { class: "msg error" }, t("saves.failed", { f: x.file, e: errorText(x.error) })));
    }
    if (r.reload) lines.push(el("div", { class: "msg" }, reloaded(r.reload)));
    report.replaceChildren(...lines);
  };

  const view: SavesView = {
    render(root) {
      const pick = el("button", { class: "btn primary" }, t("saves.pick"));
      pick.onclick = async () => {
        const chosen = await open({ multiple: true, filters: [{ name: "Saves", extensions: ["sav", "srm", "SAV", "SRM"] }] });
        if (!chosen) return;
        await view.upload(Array.isArray(chosen) ? chosen : [chosen]);
      };
      const reload = el("button", { class: "btn" }, t("saves.reload"));
      reload.onclick = () =>
        busy(reload, async () => {
          try {
            toast(reloaded(await api.reload()));
            loadList();
          } catch (e) {
            toast(errorText(e), true);
          }
        });
      dropText = el("span", { class: "title" }, t("saves.drop"));
      drop = el("div", { class: "drop" }, dropText, pick);
      report = el("div", { class: "stack tight" });
      list = el("div");
      root.replaceChildren(
        header(t("saves.title"), reload),
        drop,
        report,
        el("div", { class: "card flush" }, list),
        el("p", { class: "muted tiny" }, t("saves.limit")),
      );
      loadList();
    },
    async upload(paths) {
      if (uploading || !paths.length) return;
      uploading = true;
      report.replaceChildren(el("div", { class: "msg", id: "upload-progress" }, t("saves.uploading", { i: 1, n: paths.length, f: paths[0].split(/[\\/]/).pop()! })));
      try {
        showReport(await api.upload(paths));
      } catch (e) {
        report.replaceChildren(errorBox(e));
      } finally {
        uploading = false;
      }
      loadList();
    },
    setDragOver(over) {
      drop?.classList.toggle("over", over);
      if (dropText) dropText.textContent = t(over ? "saves.drop_over" : "saves.drop");
    },
  };
  return view;
}

export function onUploadProgress(i: number, n: number, file: string) {
  const msg = document.getElementById("upload-progress");
  if (msg) msg.textContent = t("saves.uploading", { i: i + 1, n, f: file });
}
