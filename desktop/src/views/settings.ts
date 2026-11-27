// Ajustes do CYD (D06/D06b/D06c): Tela e som + a cidade do pôr do sol, e
// Vitrine e navegação. Lê e muda pelo GET/PUT /settings (api 6); a cidade
// vai pelo /location (api 3) como antes.
import { api, errorText, type ApiError, type Config, type CydSettings, type Location, type Status } from "../api";
import { busy, el, toast } from "../dom";
import { t, type Key } from "../i18n";
import { header, segmented, settingRow, toggle, type View } from "./common";

/// Busca no Open-Meteo (pelo Rust), grava e o status manda pro CYDex.
/// `draw` repinta quando chega status novo.
function locationCard(cfg: Config, getStatus: () => Status) {
  let location: Location | null = cfg.location ?? null;
  const current = el("div", { class: "stack tight" });
  const results = el("div", { class: "found" });
  const input = el("input", { type: "text", placeholder: t("loc.placeholder"), "aria-label": t("loc.placeholder"), spellcheck: "false", autocomplete: "off", class: "grow" });

  const draw = (st: Status) => {
    const lines: HTMLElement[] = [];
    const fwOld = st.online && st.info && st.info.api < 3;
    if (!location) {
      lines.push(el("div", { class: "muted small" }, t("loc.unset")));
    } else {
      const remove = el("button", { class: "btn ghost sm" }, t("loc.remove"));
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
      const sun = st.sun;
      let info: string;
      if (fwOld) info = t("loc.old_fw");
      else if (!st.online || !sun || sun.lat === null) info = t("loc.pending");
      else if (sun.night === null) info = t("loc.no_clock");
      else if (sun.rise && sun.set) info = t("loc.times", { r: sun.rise, s: sun.set });
      else info = t("loc.polar");
      lines.push(el("div", { class: "row" }, el("strong", {}, location.name), el("span", { class: "muted small grow" }, info), remove));
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
            const b = el("button", { class: "btn ghost sm" }, place.name);
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
    "section",
    { class: "card stack" },
    el("div", {}, el("h2", {}, t("loc.title")), el("div", { class: "muted tiny" }, t("loc.hint"))),
    current,
    el("div", { class: "row" }, input, searchBtn),
    results,
  );
  return { card, draw };
}

type Tab = "display" | "showcase";
// Fica entre uma visita e outra à seção.
let tab: Tab = "display";

function intervalLabel(s: number): string {
  return s < 60 ? `${s} s` : `${s / 60} min`;
}

export function settingsView(getStatus: () => Status): View {
  let root: HTMLElement;
  let loc: ReturnType<typeof locationCard> | null = null;
  let cyd: CydSettings | null = null;
  let loadedFor = "";

  const supported = (st: Status) => st.online && st.key_ok === true && (st.info?.api ?? 0) >= 6;

  /// Manda um campo; o CYD devolve todos (é ele quem valida).
  const set = async (key: keyof CydSettings, value: string | number | boolean) => {
    try {
      cyd = await api.setCydSetting(key, value);
    } catch (e) {
      toast(errorText(e), true);
      cyd = await api.cydSettings().catch(() => cyd);
    }
    draw();
  };

  const slider = (label: string, key: "bright" | "bright_n", min: number) => {
    const value = el("span", {}, `${cyd![key]}%`);
    const input = el("input", { type: "range", min, max: 100, value: cyd![key], "aria-label": label });
    input.oninput = () => (value.textContent = `${input.value}%`);
    // Grava ao soltar, como os sliders do CYD.
    input.onchange = () => set(key, input.value);
    return el("div", { class: "slider" }, input, value);
  };

  const select = (label: string, options: [string, string][], current: string, onPick: (v: string) => void) => {
    const sel = el("select", { "aria-label": label });
    for (const [v, text] of options) sel.append(el("option", { value: v, selected: v === current }, text));
    sel.onchange = () => onPick(sel.value);
    return sel;
  };

  const displayCard = (c: CydSettings, enabled: boolean) =>
    el(
      "section",
      { class: enabled ? "card rows" : "card rows disabled", "aria-label": t("settings.tab_display") },
      settingRow(t("settings.bright"), null, slider(t("settings.bright"), "bright", 10)),
      settingRow(t("settings.bright_n"), null, slider(t("settings.bright_n"), "bright_n", 5)),
      settingRow(
        t("settings.off"),
        null,
        select(
          t("settings.off"),
          c.off_opts.map((m) => [String(m), m === 0 ? t("settings.off_never") : t("settings.off_after", { n: m })]),
          String(c.off_min),
          (v) => set("off_min", v),
        ),
      ),
      settingRow(t("settings.sound"), null, toggle(c.sound, t("settings.sound"), (on) => set("sound", on))),
    );

  const showcaseCards = (c: CydSettings, enabled: boolean, st: Status) => {
    const cls = enabled ? "card rows" : "card rows disabled";
    const d = st.dex;
    const counts = [d?.caught, d?.seen, d?.count, undefined];
    const sources = [0, 1, 2, 3].map((i): [string, string] => {
      const name = t(`settings.src_${i}` as Key);
      return [String(i), counts[i] !== undefined ? `${name} (${counts[i]})` : name];
    });
    return [
      el(
        "section",
        { class: cls, "aria-label": t("settings.showcase") },
        el("div", { class: "group-title" }, t("settings.showcase")),
        settingRow(t("settings.sc_int"), null, segmented(t("settings.sc_int"), c.sc_int_opts.map((s) => [s, intervalLabel(s)]), c.sc_int, (v) => set("sc_int", v))),
        settingRow(
          t("settings.sc_order"),
          null,
          segmented(t("settings.sc_order"), [[false, t("settings.sc_seq")], [true, t("settings.sc_random")]], c.sc_rand, (v) => set("sc_rand", v)),
        ),
        settingRow(t("settings.sc_src"), null, select(t("settings.sc_src"), sources, String(c.sc_src), (v) => set("sc_src", v))),
      ),
      el(
        "section",
        { class: cls, "aria-label": t("settings.nav") },
        el("div", { class: "group-title" }, t("settings.nav")),
        settingRow(
          t("settings.grid"),
          null,
          segmented(t("settings.grid"), [[false, t("settings.grid_list")], [true, t("settings.grid_grid")]], c.grid, (v) => set("grid", v)),
        ),
        settingRow(
          t("settings.boot"),
          null,
          segmented(t("settings.boot"), [[false, t("settings.boot_menu")], [true, t("settings.boot_showcase")]], c.sc_boot, (v) => set("sc_boot", v)),
        ),
        settingRow(t("settings.lang"), null, select(t("settings.lang"), c.langs.map((l) => [l.code, l.name]), c.lang, (v) => set("lang", v))),
      ),
    ];
  };

  function draw() {
    const st = getStatus();
    const ok = supported(st);
    const tabs = segmented<Tab>(t("settings.tabs"), [["display", t("settings.tab_display")], ["showcase", t("settings.tab_showcase")]], tab, (v) => {
      tab = v;
      draw();
    });
    tabs.classList.add("tabs");
    const parts: HTMLElement[] = [header(t("settings.title"), el("span", { class: "muted tiny" }, t(ok ? "settings.live" : "settings.old_hint"))), tabs];
    if (st.online && st.key_ok && !ok) {
      parts.push(el("div", { class: "banner warn", role: "note" }, el("div", { class: "small" }, t("settings.old", { v: st.info?.version ?? "?", a: st.info?.api ?? "?" }))));
    } else if (!st.online) {
      parts.push(el("div", { class: "msg" }, t("settings.offline")));
    }
    // Sem /settings, os controles aparecem desativados com os valores de fábrica (D06c).
    const c: CydSettings = cyd ?? {
      bright: 70, bright_n: 30, off_min: 2, sound: true, grid: false, sc_int: 60, sc_rand: false, sc_src: 0, sc_boot: false, lang: "pt-BR",
      off_opts: [1, 2, 5, 10, 0], sc_int_opts: [30, 60, 120, 300, 900], langs: [{ code: "pt-BR", name: "Português (Brasil)" }],
    };
    const enabled = ok && cyd !== null;
    if (tab === "display") parts.push(displayCard(c, enabled), loc!.card);
    else parts.push(...showcaseCards(c, enabled, st));
    root.replaceChildren(...parts);
  }

  /// Lê do CYD quando ele aparece (ou troca de firmware).
  const load = async (st: Status) => {
    const key = supported(st) ? `${st.info?.version}` : "";
    if (key === loadedFor) return;
    loadedFor = key;
    cyd = key ? await api.cydSettings().catch((e) => (toast(errorText(e), true), null)) : null;
    draw();
  };

  return {
    onStatus: (st) => {
      loc?.draw(st);
      void load(st);
    },
    async render(r) {
      root = r;
      const cfg = await api.getConfig();
      loc = locationCard(cfg, getStatus);
      draw();
      await load(getStatus());
    },
  };
}
