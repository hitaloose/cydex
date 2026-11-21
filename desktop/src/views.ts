// As quatro abas: Dex, Saves, Avisos e Conexão.
import { ask, open } from "@tauri-apps/plugin-dialog";
import { disable, enable, isEnabled } from "@tauri-apps/plugin-autostart";
import { api, errorText, type ApiError, type Config, type Location, type NoticeInfo, type SaveInfo, type Status, type UploadReport } from "./api";
import { busy, el, svg, toast } from "./dom";
import { lang, setLang, t } from "./i18n";

export interface View {
  render(root: HTMLElement): void | Promise<void>;
  /// Status novo do backend (a cada 30 s ou depois de uma ação).
  onStatus?(st: Status): void;
}

const ROMAN = ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"];

function fmtAge(s: number): string {
  if (s < 60) return `${s} s`;
  if (s < 3600) return `${Math.floor(s / 60)} min`;
  return `${Math.floor(s / 3600)} h`;
}

function signal(rssi: number | null | undefined): string {
  if (rssi === null || rssi === undefined) return "—";
  const word = rssi >= -60 ? t("conn.signal_good") : rssi >= -72 ? t("conn.signal_ok") : t("conn.signal_bad");
  return `${word}, ${rssi} dBm`;
}

function errorBox(e: unknown) {
  return el("div", { class: "msg error" }, errorText(e));
}

// ---------- Dex ----------

export function dexView(getStatus: () => Status): View {
  let root: HTMLElement;
  const draw = (st: Status) => {
    root.replaceChildren();
    const d = st.dex;
    if (!d) {
      root.append(el("div", { class: "card muted" }, t("dex.empty")));
      return;
    }
    const pct = d.count ? d.caught / d.count : 0;
    const r = 80;
    const len = 2 * Math.PI * r;
    const ring = el(
      "div",
      { class: "ring" },
      svg(
        "svg",
        { viewBox: "0 0 180 180" },
        svg("circle", { cx: 90, cy: 90, r, fill: "none", stroke: "var(--surface-2)", "stroke-width": 14 }),
        svg("circle", {
          cx: 90,
          cy: 90,
          r,
          fill: "none",
          stroke: "var(--accent)",
          "stroke-width": 14,
          "stroke-linecap": "round",
          "stroke-dasharray": `${len * pct} ${len}`,
        }),
      ),
      el("div", { class: "center" }, el("div", { class: "pct" }, `${Math.round(pct * 100)}%`), el("div", { class: "muted small" }, t("dex.caught"))),
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
    const refreshBtn = el("button", { class: "btn ghost" }, t("dex.refresh"));
    refreshBtn.onclick = () => busy(refreshBtn, () => api.refresh());
    root.append(
      el("div", { class: "row space" }, el("h2", {}, t("dex.title")), refreshBtn),
      el(
        "div",
        { class: "card dex" },
        el("div", { class: "stack" }, ring, el("p", { class: "muted small" }, t("dex.summary", { c: d.caught, s: d.seen, t: d.count }))),
        el(
          "div",
          { class: "stack" },
          gens,
          el("div", { class: "legend" }, el("span", { class: "l-caught" }, t("dex.legend_caught")), el("span", { class: "l-seen" }, t("dex.legend_seen"))),
        ),
      ),
    );
  };
  return {
    render(r) {
      root = r;
      draw(getStatus());
    },
    onStatus: (st) => root && draw(st),
  };
}

// ---------- Saves ----------

export interface SavesView extends View {
  upload(paths: string[]): Promise<void>;
  setDragOver(over: boolean): void;
}

export function savesView(): SavesView {
  let root: HTMLElement;
  let list: HTMLElement;
  let drop: HTMLElement;
  let report: HTMLElement;
  let uploading = false;

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
      list.replaceChildren(el("p", { class: "muted" }, t("saves.empty")));
      return;
    }
    const body = el("tbody");
    for (const s of saves) {
      const del = el("button", { class: "btn ghost danger" }, t("saves.delete"));
      del.onclick = async () => {
        if (!(await ask(t("saves.delete_ask", { n: s.name }), { kind: "warning", title: "CYDex" }))) return;
        await busy(del, async () => {
          try {
            await api.deleteSave(s.name);
            const r = await api.reload();
            toast(t("saves.reloaded", { p: r.parsed, s: r.saves, c: r.caught, v: r.seen }));
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
    list.replaceChildren(el("table", {}, body));
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
    if (r.reload) {
      const rl = r.reload;
      lines.push(el("div", { class: "msg" }, t("saves.reloaded", { p: rl.parsed, s: rl.saves, c: rl.caught, v: rl.seen })));
    }
    report.replaceChildren(...lines);
  };

  const view: SavesView = {
    render(r) {
      root = r;
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
            const rl = await api.reload();
            toast(t("saves.reloaded", { p: rl.parsed, s: rl.saves, c: rl.caught, v: rl.seen }));
            loadList();
          } catch (e) {
            toast(errorText(e), true);
          }
        });
      drop = el("div", { class: "drop" }, el("div", {}, t("saves.drop")), pick);
      report = el("div", { class: "stack" });
      list = el("div");
      root.replaceChildren(
        el("div", { class: "row space" }, el("h2", {}, t("saves.title")), reload),
        el("div", { class: "stack" }, drop, report, el("div", { class: "card" }, list), el("p", { class: "muted small" }, t("saves.limit"))),
      );
      loadList();
    },
    async upload(paths) {
      if (uploading || !paths.length) return;
      uploading = true;
      report.replaceChildren(el("div", { class: "msg" }, t("saves.uploading", { i: 1, n: paths.length, f: paths[0].split(/[\\/]/).pop()! })));
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
    },
  };
  return view;
}

export function onUploadProgress(i: number, n: number, file: string) {
  const msg = document.querySelector(".drop + .stack .msg");
  if (msg) msg.textContent = t("saves.uploading", { i: i + 1, n, f: file });
}

// ---------- Avisos ----------

const TYPE_COLORS: Record<string, string> = {
  info: "var(--muted)",
  success: "var(--success)",
  warning: "var(--warning)",
  waiting: "var(--waiting)",
  question: "var(--accent)",
  error: "var(--accent)",
};

export function noticesView(): View {
  let list: HTMLElement;

  const loadList = async () => {
    let items: NoticeInfo[];
    try {
      items = await api.notices();
    } catch (e) {
      list.replaceChildren(errorBox(e));
      return;
    }
    if (!items.length) {
      list.replaceChildren(el("p", { class: "muted" }, t("notices.empty")));
      return;
    }
    list.replaceChildren(
      ...items.map((n) => {
        const close = el("button", { class: "btn ghost" }, t("notices.close"));
        close.onclick = () =>
          busy(close, async () => {
            try {
              await api.closeNotice(n.id);
            } catch (e) {
              toast(errorText(e), true);
            }
            loadList();
          });
        return el(
          "div",
          { class: "notice" },
          el("div", { class: "bar", style: `background:${TYPE_COLORS[n.type] ?? "var(--muted)"}` }),
          el(
            "div",
            { class: "grow" },
            el("div", { class: "title" }, n.title),
            el("div", { class: "muted small" }, [n.id, n.tag, t("notices.age", { n: fmtAge(n.age) })].filter(Boolean).join(" · ")),
          ),
          close,
        );
      }),
    );
  };

  return {
    render(root) {
      const closeAll = el("button", { class: "btn" }, t("notices.close_all"));
      closeAll.onclick = () =>
        busy(closeAll, async () => {
          try {
            await api.closeNotice(null);
          } catch (e) {
            toast(errorText(e), true);
          }
          loadList();
        });
      list = el("div");

      const title = el("input", { type: "text", maxlength: 60, value: t("notices.default_title") });
      const subtitle = el("input", { type: "text", maxlength: 31, value: "CYDex Desktop" });
      const message = el("textarea", { maxlength: 111 });
      const type = el("select");
      for (const k of ["info", "success", "warning", "waiting", "question", "error"] as const) {
        type.append(el("option", { value: k }, t(`type.${k}`)));
      }
      const sound = el("input", { type: "checkbox" });
      const send = el("button", { class: "btn primary" }, t("notices.send"));
      send.onclick = () =>
        busy(send, async () => {
          try {
            await api.sendNotice({
              title: title.value.trim() || t("notices.default_title"),
              subtitle: subtitle.value.trim() || undefined,
              message: message.value.trim() || undefined,
              type: type.value,
              sound: sound.checked,
              tag: "desktop",
            });
            toast(t("notices.sent"));
          } catch (e) {
            toast(errorText(e), true);
          }
          loadList();
        });

      root.replaceChildren(
        el("div", { class: "row space" }, el("h2", {}, t("notices.title")), closeAll),
        el("div", { class: "card" }, list),
        el("h3", {}, t("notices.test")),
        el(
          "div",
          { class: "card form-grid" },
          el("label", { class: "field" }, t("notices.field_title"), title),
          el("label", { class: "field" }, t("notices.field_subtitle"), subtitle),
          el("label", { class: "field full" }, t("notices.field_message"), message),
          el("label", { class: "field" }, t("notices.field_type"), type),
          el("label", { class: "check", style: "align-self:end;min-height:34px" }, sound, t("notices.field_sound")),
          el("div", { class: "full row" }, send),
        ),
      );
      loadList();
    },
  };
}

// ---------- Conexão ----------

// ---------- Localidade (brilho de dia/noite) ----------

/// Cartão da cidade: busca no Open-Meteo (pelo Rust), grava e o status manda
/// pro CYDex. `draw` repinta quando chega status novo.
function locationCard(cfg: Config, getStatus: () => Status) {
  let location: Location | null = cfg.location ?? null;
  const current = el("div", { class: "stack" });
  const results = el("div", { class: "found" });
  const input = el("input", { type: "text", placeholder: t("loc.placeholder"), spellcheck: "false", autocomplete: "off" });
  input.style.flex = "1";

  const draw = (st: Status) => {
    const lines: HTMLElement[] = [];
    const fwOld = st.online && st.info && st.info.api < 3;
    if (!location) {
      lines.push(el("div", { class: "muted small" }, t("loc.unset")));
    } else {
      const remove = el("button", { class: "btn ghost" }, t("loc.remove"));
      remove.onclick = () =>
        busy(remove, async () => {
          try {
            await api.clearLocation();
            location = null;
            toast(t("loc.removed"));
            draw(getStatus());
          } catch (e) {
            toast(errorText(e), true);
          }
        });
      lines.push(el("div", { class: "row" }, el("strong", {}, location.name), remove));
      const sun = st.sun;
      let info: string;
      if (fwOld) info = t("loc.old_fw");
      else if (!st.online || !sun || sun.lat === null) info = t("loc.pending");
      else if (sun.night === null) info = t("loc.no_clock");
      else if (sun.rise && sun.set) info = t("loc.times", { r: sun.rise, s: sun.set });
      else info = t("loc.polar");
      lines.push(el("div", { class: "msg" }, info));
    }
    current.replaceChildren(...lines);
  };

  const searchBtn = el("button", { class: "btn" }, t("loc.search"));
  const search = () =>
    busy(searchBtn, async () => {
      results.replaceChildren(el("span", { class: "muted small" }, t("loc.searching")));
      try {
        const list = await api.searchPlaces(input.value);
        if (!list.length) {
          results.replaceChildren(el("span", { class: "muted small" }, t("loc.none")));
          return;
        }
        results.replaceChildren(
          ...list.map((place) => {
            const b = el("button", { class: "btn ghost" }, place.name);
            b.onclick = () =>
              busy(b, async () => {
                try {
                  const st = await api.setLocation(place);
                  location = place;
                  results.replaceChildren();
                  input.value = "";
                  if (st.sun && st.sun.lat !== null) toast(t("loc.saved"));
                  draw(st);
                } catch (e) {
                  toast(errorText(e), true);
                }
              });
            return b;
          }),
        );
      } catch (e) {
        const err = e as ApiError;
        results.replaceChildren(el("div", { class: "msg error" }, t("loc.search_err", { d: err?.detail ?? String(e) })));
      }
    });
  searchBtn.onclick = search;
  input.onkeydown = (ev) => {
    if (ev.key === "Enter") search();
  };

  draw(getStatus());
  const card = el(
    "div",
    { class: "card stack" },
    el("div", { class: "muted small" }, t("loc.hint")),
    current,
    el("div", { class: "row" }, input, searchBtn),
    results,
  );
  return { card, draw };
}

export function connectionView(onLangChange: () => void, getStatus: () => Status): View {
  let loc: ReturnType<typeof locationCard> | null = null;
  return {
    onStatus: (st) => loc?.draw(st),
    async render(root) {
      const cfg: Config = await api.getConfig();
      const host = el("input", { type: "text", value: cfg.host, spellcheck: "false", autocomplete: "off" });
      const key = el("input", { type: "password", value: cfg.key, spellcheck: "false", autocomplete: "off", maxlength: 47 });
      const result = el("div");
      const found = el("div", { class: "found" });

      const findBtn = el("button", { class: "btn" }, t("conn.find"));
      findBtn.onclick = () =>
        busy(findBtn, async () => {
          found.replaceChildren(el("span", { class: "muted small" }, t("conn.finding")));
          const list = await api.discover();
          if (!list.length) {
            found.replaceChildren(el("span", { class: "muted small" }, t("conn.found_none")));
            return;
          }
          found.replaceChildren(
            ...list.flatMap((f) =>
              [f.host, f.ip].map((addr) => {
                const b = el("button", { class: "btn ghost" }, addr);
                b.onclick = () => (host.value = addr);
                return b;
              }),
            ),
          );
        });

      const test = async () => {
        try {
          const r = await api.test(host.value, key.value);
          const vars = { n: r.info.name, v: r.info.version, r: signal(r.info.rssi) };
          const lines = [el("div", { class: r.key_ok ? "msg ok" : "msg error" }, t(r.key_ok ? "conn.ok" : "conn.ok_no_key", vars))];
          if (r.info.api < 2) lines.push(el("div", { class: "msg" }, t("conn.old_fw")));
          result.replaceChildren(...lines);
        } catch (e) {
          result.replaceChildren(errorBox(e));
        }
      };
      const testBtn = el("button", { class: "btn" }, t("conn.test"));
      testBtn.onclick = () => busy(testBtn, test);

      const sync = el("input", { type: "checkbox", checked: cfg.power_sync });
      const auto = el("input", { type: "checkbox" });
      isEnabled()
        .then((on) => (auto.checked = on))
        .catch(() => (auto.disabled = true));
      auto.onchange = async () => {
        try {
          await (auto.checked ? enable() : disable());
        } catch (e) {
          auto.checked = !auto.checked;
          toast(String(e), true);
        }
      };
      const langSel = el("select");
      for (const [v, label] of [["", t("conn.lang_auto")], ["pt-BR", "Português (Brasil)"], ["en", "English"]]) {
        langSel.append(el("option", { value: v, selected: cfg.lang === v }, label));
      }

      const save = el("button", { class: "btn primary" }, t("conn.save"));
      save.onclick = () =>
        busy(save, async () => {
          const next = await api.saveConfig({ ...cfg, host: host.value, key: key.value, power_sync: sync.checked, lang: langSel.value });
          host.value = next.host;
          key.value = next.key;
          const langChanged = next.lang !== cfg.lang;
          cfg.lang = next.lang;
          toast(t("conn.saved"));
          await test();
          if (langChanged) {
            setLang(await api.getLang());
            onLangChange();
          }
        });

      const sleepBtn = el("button", { class: "btn" }, t("conn.sleep"));
      const wakeBtn = el("button", { class: "btn" }, t("conn.wake"));
      for (const [b, s] of [[sleepBtn, true], [wakeBtn, false]] as const) {
        b.onclick = () =>
          busy(b, async () => {
            try {
              await api.power(s);
            } catch (e) {
              toast(errorText(e), true);
            }
          });
      }

      root.replaceChildren(
        el("h2", {}, t("conn.title")),
        el(
          "div",
          { class: "card stack" },
          el("label", { class: "field" }, t("conn.host"), el("span", { class: "hint" }, t("conn.host_hint")), el("div", { class: "row" }, host, findBtn)),
          found,
          el("label", { class: "field" }, t("conn.key"), el("span", { class: "hint" }, t("conn.key_hint")), key),
          el("div", { class: "row" }, save, testBtn),
          result,
        ),
        el("h3", {}, t("conn.options")),
        el(
          "div",
          { class: "card stack" },
          el("label", { class: "check" }, sync, t("conn.power_sync")),
          el("div", { class: "row small muted" }, t("conn.power_test"), sleepBtn, wakeBtn),
          el("label", { class: "check" }, auto, t("conn.autostart")),
          el("label", { class: "field" }, t("conn.lang"), langSel),
        ),
        el("h3", {}, t("loc.title")),
        (loc = locationCard(cfg, getStatus)).card,
      );
      host.style.flex = "1";
      if (cfg.key) test();
    },
  };
}
