// Avisos (D03): abertos, indicadores do rodapé da Vitrine e aviso de teste.
import { api, errorText, type IndicatorInfo, type NoticeInfo, type Status } from "../api";
import { busy, el, toast } from "../dom";
import { t } from "../i18n";
import { CHEVRON } from "./home";
import { errorBox, fmtAge, header, type View } from "./common";

const TYPE_COLORS: Record<string, string> = {
  info: "var(--muted)",
  success: "var(--success)",
  warning: "var(--warning)",
  waiting: "var(--waiting)",
  question: "var(--accent)",
  error: "var(--accent)",
};

export function noticesView(getStatus: () => Status): View {
  let list: HTMLElement;
  let indicators: HTMLElement;

  const loadList = async () => {
    let items: NoticeInfo[];
    try {
      items = await api.notices();
    } catch (e) {
      list.replaceChildren(errorBox(e));
      return;
    }
    if (!items.length) {
      list.replaceChildren(el("p", { class: "muted pad" }, t("notices.empty")));
      return;
    }
    list.replaceChildren(
      ...items.map((n) => {
        const close = el("button", { class: "btn ghost sm" }, t("notices.close"));
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
            el("div", { class: "muted tiny" }, [n.id, n.tag, t("notices.age", { n: fmtAge(n.age) })].filter(Boolean).join(" · ")),
          ),
          close,
        );
      }),
    );
  };

  // GET /status só existe com api ≥ 4: sem ele, a seção nem aparece.
  const loadIndicators = async () => {
    const api4 = (getStatus().info?.api ?? 0) >= 4;
    indicators.hidden = !api4;
    if (!api4) return;
    let items: IndicatorInfo[];
    try {
      items = await api.indicators();
    } catch {
      indicators.hidden = true;
      return;
    }
    const body = items.length
      ? el(
          "div",
          { class: "grid2" },
          ...items.map((s) =>
            el(
              "div",
              { class: "card chipcard" },
              el("span", { class: "title" }, s.text),
              el("span", { class: "muted tiny" }, `${s.id} · ${t("notices.age", { n: fmtAge(s.age) })}`),
            ),
          ),
        )
      : el("p", { class: "muted small" }, t("notices.indicators_empty"));
    indicators.replaceChildren(el("h2", {}, t("notices.indicators")), body);
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
      indicators = el("div", { class: "stack tight", hidden: true });

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
        header(t("notices.title"), closeAll),
        el("div", { class: "card list" }, list),
        indicators,
        el(
          "details",
          { class: "card flush" },
          el("summary", {}, CHEVRON(), t("notices.test")),
          el(
            "div",
            { class: "form-grid" },
            el("label", { class: "field" }, t("notices.field_title"), title),
            el("label", { class: "field" }, t("notices.field_subtitle"), subtitle),
            el("label", { class: "field full" }, t("notices.field_message"), message),
            el("label", { class: "field" }, t("notices.field_type"), type),
            el("label", { class: "check", style: "align-self:end;min-height:34px" }, sound, t("notices.field_sound")),
            el("div", { class: "full row" }, send),
          ),
        ),
      );
      loadList();
      loadIndicators();
    },
  };
}
