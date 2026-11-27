// Ajustes do CYD (D06). Por enquanto só a cidade do pôr do sol: o resto
// (brilho, vitrine, navegação, idioma) espera a rota /settings no firmware.
import { api, errorText, type ApiError, type Config, type Location, type Status } from "../api";
import { busy, el, toast } from "../dom";
import { t } from "../i18n";
import { header, type View } from "./common";

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

export function settingsView(getStatus: () => Status): View {
  let loc: ReturnType<typeof locationCard> | null = null;
  return {
    onStatus: (st) => loc?.draw(st),
    async render(root) {
      const cfg = await api.getConfig();
      loc = locationCard(cfg, getStatus);
      root.replaceChildren(header(t("settings.title")), loc.card, el("div", { class: "msg" }, t("settings.later")));
    },
  };
}
