// Tela e energia (D04): apagar/acender o CYD junto com o computador.
import { api, errorText } from "../api";
import { busy, el, svg, toast } from "../dom";
import { t } from "../i18n";
import { header, toggle, type View } from "./common";

/// Título e linha do card "Tela do CYD" do Início e do pé da barra.
export function screenSummary(sync: boolean): [string, string] {
  return sync ? [t("power.on"), t("power.on_hint")] : [t("power.off"), t("power.off_hint")];
}

export function powerView(onChange: () => void): View {
  return {
    async render(root) {
      const cfg = await api.getConfig();
      const openLog = el("button", { class: "btn ghost sm" }, t("power.open_log"));
      openLog.onclick = () => api.openLog(false).catch((e) => toast(String(e), true));

      const state = el("section", { class: "card now" });
      const drawState = (sync: boolean) => {
        const [title, sub] = screenSummary(sync);
        const buttons = (["power.sleep", "power.wake"] as const).map((k) => {
          const b = el("button", { class: "btn sm" }, t(k));
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
        state.replaceChildren(
          el(
            "div",
            { class: sync ? "icon-box on" : "icon-box" },
            svg(
              "svg",
              { width: 22, height: 22, viewBox: "0 0 24 24", fill: "none", stroke: "currentColor", "stroke-width": 2.2, "stroke-linecap": "round", "stroke-linejoin": "round", "aria-hidden": "true" },
              svg("rect", { x: 3, y: 4, width: 18, height: 13, rx: 2 }),
              svg("path", { d: "M8 21h8M12 17v4" }),
            ),
          ),
          el("div", { class: "grow" }, el("div", { class: "big" }, title), el("div", { class: "muted small" }, sub)),
          el("div", { class: "stack tight" }, ...buttons),
        );
      };

      const sync = toggle(cfg.power_sync, t("power.sync"), async (on) => {
        await api.saveConfig({ ...(await api.getConfig()), power_sync: on });
        drawState(on);
        onChange();
      });
      drawState(cfg.power_sync);
      root.replaceChildren(
        header(t("power.title"), openLog),
        el(
          "section",
          { class: "card setting" },
          el("div", { class: "grow" }, el("div", { class: "title" }, t("power.sync")), el("div", { class: "muted tiny" }, t("power.sync_hint"))),
          sync,
        ),
        state,
      );
    },
  };
}
