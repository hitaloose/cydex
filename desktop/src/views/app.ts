// Este app (D07): abrir com o sistema, idioma da janela, log.
import { getVersion } from "@tauri-apps/api/app";
import { disable, enable, isEnabled } from "@tauri-apps/plugin-autostart";
import { api } from "../api";
import { el, toast } from "../dom";
import { setLang, t } from "../i18n";
import { header, settingRow, toggle, type View } from "./common";

export function appView(onLangChange: () => void): View {
  return {
    async render(root) {
      const cfg = await api.getConfig();
      const auto = toggle(false, t("app.autostart"), (on) => (on ? enable() : disable()));
      isEnabled()
        .then((on) => auto.setAttribute("aria-pressed", on ? "true" : "false"))
        .catch(() => (auto.disabled = true));

      const langSel = el("select", { "aria-label": t("app.lang") });
      for (const [v, label] of [["", t("app.lang_auto")], ["pt-BR", "Português (Brasil)"], ["en", "English"]]) {
        langSel.append(el("option", { value: v, selected: cfg.lang === v }, label));
      }
      langSel.onchange = async () => {
        await api.saveConfig({ ...(await api.getConfig()), lang: langSel.value });
        setLang(await api.getLang());
        onLangChange();
      };

      const openLog = el("button", { class: "btn sm" }, t("app.open_log"));
      openLog.onclick = () => api.openLog(false).catch((e) => toast(String(e), true));
      const reveal = el("button", { class: "btn ghost sm" }, t("app.reveal_log"));
      reveal.onclick = () => api.openLog(true).catch((e) => toast(String(e), true));
      const path = el("code", { class: "path grow" }, await api.logPath().catch(() => "—"));
      const footer = el("p", { class: "muted tiny" });
      getVersion().then((v) => (footer.textContent = t("app.footer", { v })));

      root.replaceChildren(
        header(t("app.title")),
        el("section", { class: "card rows" }, settingRow(t("app.autostart"), t("app.autostart_hint"), auto), settingRow(t("app.lang"), null, langSel)),
        el(
          "section",
          { class: "card stack tight" },
          el("h2", {}, t("app.diag")),
          el("div", { class: "row" }, path, openLog, reveal),
          el("div", { class: "muted tiny" }, t("app.diag_hint")),
        ),
        el("div", { class: "grow" }),
        footer,
      );
    },
  };
}
