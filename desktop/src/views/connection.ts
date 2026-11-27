// Conexão (D05) e primeira vez sem chave (D05b).
import { api, type Status } from "../api";
import { busy, el, toast } from "../dom";
import { t } from "../i18n";
import { errorBox, header, signal, type View } from "./common";

export function connectionView(getStatus: () => Status): View {
  let how: HTMLElement | null = null;

  const drawHow = (st: Status) => {
    if (!how) return;
    const rows: [string, string][] = [];
    if (st.info) rows.push([t("conn.firmware"), t("conn.fw_value", { v: st.info.version, a: st.info.api })]);
    how.hidden = !rows.length;
    how.replaceChildren(
      el("h2", {}, t("conn.how")),
      el("dl", { class: "facts" }, ...rows.flatMap(([k, v]) => [el("dt", {}, k), el("dd", {}, v)])),
    );
  };

  return {
    onStatus: drawHow,
    async render(root) {
      const cfg = await api.getConfig();
      const first = !cfg.key;
      const host = el("input", { type: "text", value: cfg.host, spellcheck: "false", autocomplete: "off", class: "grow" });
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
                const b = el("button", { class: "btn ghost sm" }, addr);
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

      const save = el("button", { class: "btn primary" }, t(first ? "conn.save_test" : "conn.save"));
      save.onclick = () =>
        busy(save, async () => {
          const next = await api.saveConfig({ ...(await api.getConfig()), host: host.value, key: key.value });
          host.value = next.host;
          key.value = next.key;
          toast(t("conn.saved"));
          await test();
        });

      const parts: HTMLElement[] = [header(t(first ? "conn.first_title" : "conn.title"))];
      if (first) {
        parts.push(
          el(
            "ol",
            { class: "steps" },
            ...(["conn.step1", "conn.step2", "conn.step3"] as const).map((k, i) => el("li", { class: "card" }, el("span", { class: "n" }, String(i + 1)), el("span", {}, t(k)))),
          ),
        );
      }
      parts.push(
        el(
          "section",
          { class: "card stack" },
          el("label", { class: "field" }, t("conn.host"), el("span", { class: "hint" }, t("conn.host_hint")), el("div", { class: "row" }, host, findBtn)),
          found,
          el("label", { class: "field" }, t("conn.key"), el("span", { class: "hint" }, t("conn.key_hint")), key),
          el("div", { class: "row" }, save, testBtn),
          result,
        ),
      );
      how = el("section", { class: "card stack tight", hidden: true });
      parts.push(how);
      root.replaceChildren(...parts);
      drawHow(getStatus());
      if (cfg.key) test();
    },
  };
}
