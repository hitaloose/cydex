#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "logic/TextFit.h"
#include "net/HttpRequest.h"
#include "net/WifiList.h"
#include "notify/NoticeJson.h"
#include "notify/Notice.h"
#include "notify/Status.h"

void setUp() {}
void tearDown() {}

// ---------- TextFit ----------

void test_fit_collapses_whitespace_and_markdown() {
  char out[64];
  text_fit("  I've **completed** the\n\n`refactor`.  ", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("I've completed the refactor.", out);
}

void test_fit_keeps_latin1_and_substitutes() {
  char out[64];
  text_fit("Ação \xE2\x80\x98ok\xE2\x80\x99 a \xE2\x86\x92 b", out, sizeof(out));  // ‘ok’ →
  TEST_ASSERT_EQUAL_STRING("Ação 'ok' a -> b", out);
}

void test_fit_drops_emoji_and_cjk() {
  char out[64];
  text_fit("Pronto \xF0\x9F\x8E\x89 \xE6\x97\xA5!", out, sizeof(out));  // 🎉 日
  TEST_ASSERT_EQUAL_STRING("Pronto !", out);
}

void test_fit_truncates_with_ellipsis_on_char_boundary() {
  char out[12];  // 11 bytes úteis
  size_t n = text_fit("ééééééééé", out, sizeof(out));
  // "…" tem 3 bytes: cabem 4 "é" (8 bytes) + "…".
  TEST_ASSERT_EQUAL_STRING("éééé\xE2\x80\xA6", out);
  TEST_ASSERT_EQUAL(11, n);
}

void test_fit_exact_fit_has_no_ellipsis() {
  char out[6];
  text_fit("abcde", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("abcde", out);
}

void test_fit_null_and_empty() {
  char out[8] = "x";
  TEST_ASSERT_EQUAL(0, text_fit(nullptr, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
  text_fit("   \n ", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("", out);
}

// ---------- NoticeJson ----------

static NoticeFields parse(const std::string &body, size_t chunk = 7) {
  NoticeJson j;
  for (size_t i = 0; i < body.size(); i += chunk) {
    j.feed(body.data() + i, std::min(chunk, body.size() - i));
  }
  TEST_ASSERT_TRUE_MESSAGE(j.done(), "JSON não terminou");
  return j.fields();
}

void test_json_all_fields() {
  NoticeFields f = parse(R"({"id":"ci:cydex","tag":"build","title":"Build falhou","subtitle":"cydex · main",
    "message":"test_i18n","type":"error","priority":"normal","icon":"bell","color":"#112233",
    "timeout":600,"sound":false,"wake":true})");
  TEST_ASSERT_EQUAL_STRING("ci:cydex", f.id);
  TEST_ASSERT_EQUAL_STRING("build", f.tag);
  TEST_ASSERT_EQUAL_STRING("Build falhou", f.title);
  TEST_ASSERT_EQUAL_STRING("cydex · main", f.subtitle);
  TEST_ASSERT_EQUAL_STRING("test_i18n", f.message);
  TEST_ASSERT_EQUAL_STRING("error", f.type);
  TEST_ASSERT_EQUAL_STRING("normal", f.priority);
  TEST_ASSERT_EQUAL_STRING("bell", f.icon);
  TEST_ASSERT_EQUAL_STRING("#112233", f.color);
  TEST_ASSERT_TRUE(f.has_timeout);
  TEST_ASSERT_EQUAL(600, f.timeout);
  TEST_ASSERT_TRUE(f.has_sound);
  TEST_ASSERT_FALSE(f.sound);
  TEST_ASSERT_TRUE(f.has_wake);
  TEST_ASSERT_TRUE(f.wake);
  TEST_ASSERT_FALSE(f.timeout_bad || f.sound_bad || f.wake_bad);
}

void test_json_skips_unknown_and_nested() {
  NoticeFields f = parse(R"({"extra":true,"n":-12.5e3,"x":null,
    "data":[{"id":"nao","title":"}]\""}],"obj":{},
    "title":"ok","timeout":5})", 3);
  TEST_ASSERT_EQUAL_STRING("ok", f.title);
  TEST_ASSERT_EQUAL_STRING("", f.id);  // o "id" aninhado não conta
  TEST_ASSERT_EQUAL(5, f.timeout);
}

void test_json_scalar_at_end_and_spaces() {
  NoticeFields f = parse("{\"title\":\"a\", \"sound\" : true }");
  TEST_ASSERT_TRUE(f.has_sound && f.sound);
  f = parse("{\"title\":\"a\",\"timeout\":30}");
  TEST_ASSERT_EQUAL(30, f.timeout);
  f = parse("{\"title\":\"a\",\"timeout\":null}");
  TEST_ASSERT_FALSE(f.has_timeout);
}

void test_json_bad_scalars() {
  TEST_ASSERT_TRUE(parse(R"({"timeout":"10"})").timeout_bad);
  TEST_ASSERT_TRUE(parse(R"({"timeout":1.5})").timeout_bad);
  TEST_ASSERT_TRUE(parse(R"({"timeout":true})").timeout_bad);
  TEST_ASSERT_TRUE(parse(R"({"timeout":12345678901234567890})").timeout_bad);
  TEST_ASSERT_TRUE(parse(R"({"sound":1})").sound_bad);
  TEST_ASSERT_TRUE(parse(R"({"wake":"no"})").wake_bad);
  TEST_ASSERT_TRUE(parse(R"({"wake":{}})").wake_bad);
}

void test_json_escapes_and_unicode() {
  NoticeFields f = parse(R"({"message":"a\"b\\c\nd é 🎉"})");
  TEST_ASSERT_EQUAL_STRING("a\"b\\c\nd é \xF0\x9F\x8E\x89", f.message);
}

void test_json_long_value_keeps_beginning() {
  std::string big(50000, 'x');
  NoticeFields f = parse("{\"message\":\"start" + big + "\",\"title\":\"t\"}", 997);
  TEST_ASSERT_EQUAL(sizeof(f.message) - 1, strlen(f.message));
  TEST_ASSERT_EQUAL(0, strncmp(f.message, "startxxx", 8));
  TEST_ASSERT_EQUAL_STRING("t", f.title);
}

void test_json_cut_never_splits_utf8() {
  // Raw (sem escape) e escapado: "é" tem 2 bytes; o buffer de 96 corta num limite.
  std::string raw, esc;
  for (int i = 0; i < 60; i++) {
    raw += "é";
    esc += "\\u00e9";
  }
  for (const std::string &v : {std::string("a") + raw, std::string("a") + esc}) {
    NoticeFields f = parse("{\"title\":\"" + v + "\"}");
    size_t n = strlen(f.title);
    TEST_ASSERT_EQUAL(95, n);  // "a" + 47 "é"
    TEST_ASSERT_EQUAL_HEX8(0xA9, (unsigned char)f.title[n - 1]);
  }
}

void test_json_invalid() {
  NoticeJson j;
  j.feed("[1,2]", 5);
  TEST_ASSERT_TRUE(j.failed());
  j.reset();
  j.feed("{\"a\" 1}", 7);
  TEST_ASSERT_TRUE(j.failed());
  j.reset();
  j.feed("{\"a\":\"incompleto", 16);
  TEST_ASSERT_FALSE(j.done());
  TEST_ASSERT_FALSE(j.failed());
}

// ---------- Notice ----------

static NoticeError build(const std::string &body, Notice *n) {
  return notice_from_fields(parse(body), n);
}

void test_notice_defaults_info() {
  Notice n;
  TEST_ASSERT_EQUAL(NoticeError::None, build(R"({"title":"Olá"})", &n));
  TEST_ASSERT_EQUAL_STRING("Olá", n.title);
  TEST_ASSERT_EQUAL_STRING("", n.id);
  TEST_ASSERT_EQUAL_STRING("info", notice_type_name(n));
  TEST_ASSERT_EQUAL(NoticeIcon::Info, n.icon);
  TEST_ASSERT_EQUAL(NoticePriority::Low, n.priority);
  TEST_ASSERT_TRUE(n.sound);
  TEST_ASSERT_TRUE(n.wake);
  TEST_ASSERT_EQUAL(0, n.timeout_s);
}

void test_notice_presets_and_overrides() {
  Notice n;
  TEST_ASSERT_EQUAL(NoticeError::None, build(R"({"title":"t","type":"question"})", &n));
  TEST_ASSERT_EQUAL(NoticeIcon::Shield, n.icon);
  TEST_ASSERT_EQUAL(NoticePriority::High, n.priority);
  TEST_ASSERT_EQUAL_HEX32(0xFF5A4E, n.color);
  TEST_ASSERT_EQUAL(NoticeError::None,
                    build(R"({"title":"t","type":"waiting","icon":"bell","color":"#0a0B0c","priority":"low"})", &n));
  TEST_ASSERT_EQUAL_STRING("waiting", notice_type_name(n));
  TEST_ASSERT_EQUAL(NoticeIcon::Bell, n.icon);
  TEST_ASSERT_EQUAL_HEX32(0x0A0B0C, n.color);
  TEST_ASSERT_EQUAL(NoticePriority::Low, n.priority);
  // Ícone desconhecido: o do preset.
  TEST_ASSERT_EQUAL(NoticeError::None, build(R"({"title":"t","type":"success","icon":"rocket"})", &n));
  TEST_ASSERT_EQUAL(NoticeIcon::Check, n.icon);
}

void test_notice_text_is_fitted() {
  Notice n;
  TEST_ASSERT_EQUAL(NoticeError::None,
                    build(R"({"title":"**Pronto** 🎉","subtitle":"  a  b ","message":"`x`\n\ny"})", &n));
  TEST_ASSERT_EQUAL_STRING("Pronto", n.title);
  TEST_ASSERT_EQUAL_STRING("a b", n.subtitle);
  TEST_ASSERT_EQUAL_STRING("x y", n.message);
}

void test_notice_errors() {
  Notice n;
  TEST_ASSERT_EQUAL(NoticeError::Title, build(R"({})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Title, build(R"({"title":"  "})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Id, build(R"({"title":"t","id":"a/b"})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Id,
                    build(R"({"title":"t","id":"12345678901234567890123456789012345678901"})", &n));  // 41
  TEST_ASSERT_EQUAL(NoticeError::None,
                    build(R"({"title":"t","id":"1234567890123456789012345678901234567890"})", &n));  // 40
  TEST_ASSERT_EQUAL(NoticeError::Tag, build(R"({"title":"t","tag":"a b"})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Type, build(R"({"title":"t","type":"fatal"})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Priority, build(R"({"title":"t","priority":"urgent"})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Color, build(R"({"title":"t","color":"red"})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Color, build(R"({"title":"t","color":"#12345G"})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Timeout, build(R"({"title":"t","timeout":-1})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Timeout, build(R"({"title":"t","timeout":86401})", &n));
  TEST_ASSERT_EQUAL(NoticeError::None, build(R"({"title":"t","timeout":86400})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Sound, build(R"({"title":"t","sound":"yes"})", &n));
  TEST_ASSERT_EQUAL(NoticeError::Wake, build(R"({"title":"t","wake":0})", &n));
  TEST_ASSERT_EQUAL_STRING("color", notice_error_field(NoticeError::Color));
}

static Notice make(const char *id, NoticePriority p = NoticePriority::Low, const char *tag = "",
                   const char *title = "t", uint32_t timeout_s = 0) {
  Notice n;
  memset(&n, 0, sizeof(n));
  snprintf(n.id, sizeof(n.id), "%s", id);
  snprintf(n.tag, sizeof(n.tag), "%s", tag);
  snprintf(n.title, sizeof(n.title), "%s", title);
  n.priority = p;
  n.timeout_s = timeout_s;
  return n;
}

static PostResult post(NoticeBoard &q, Notice n, uint32_t now) { return q.post(n, now); }

void test_board_auto_id() {
  NoticeBoard q;
  Notice a = make("", NoticePriority::Low, "", "a");
  Notice b = make("", NoticePriority::Low, "", "b");
  TEST_ASSERT_EQUAL(PostResult::Added, q.post(a, 0));
  TEST_ASSERT_EQUAL(PostResult::Added, q.post(b, 10));
  TEST_ASSERT_EQUAL_STRING("n1", a.id);
  TEST_ASSERT_EQUAL_STRING("n2", b.id);
  TEST_ASSERT_EQUAL(2, q.size());
  // Mesmo título sem id logo depois: repetido, devolve o id que ficou.
  Notice b2 = make("", NoticePriority::Low, "", "b");
  TEST_ASSERT_EQUAL(PostResult::Repeated, q.post(b2, 20));
  TEST_ASSERT_EQUAL_STRING("n2", b2.id);
}

void test_board_replace_by_id() {
  NoticeBoard q;
  post(q, make("s1", NoticePriority::High, "permission", "perm"), 0);
  post(q, make("s2", NoticePriority::Low, "done", "done"), 10);
  TEST_ASSERT_EQUAL(PostResult::Added, post(q, make("s1", NoticePriority::Low, "done", "done"), 20));
  TEST_ASSERT_EQUAL(2, q.size());
  TEST_ASSERT_EQUAL_STRING("s2", q.at(0).id);  // empate: o mais antigo antes
  TEST_ASSERT_EQUAL_STRING("s1", q.at(1).id);
}

void test_board_remove_with_tag() {
  NoticeBoard q;
  post(q, make("s1", NoticePriority::High, "permission"), 0);
  post(q, make("s2", NoticePriority::Normal, "idle"), 10);
  TEST_ASSERT_EQUAL(0, q.remove("s2", "permission"));  // tag não bate
  TEST_ASSERT_EQUAL(1, q.remove("s1", "permission"));
  TEST_ASSERT_EQUAL(0, q.remove("s1", ""));            // já saiu: idempotente
  TEST_ASSERT_EQUAL(1, q.remove("s2", ""));
  TEST_ASSERT_EQUAL(0, q.size());
  // Fechar libera o repetido.
  TEST_ASSERT_EQUAL(PostResult::Added, post(q, make("s2", NoticePriority::Normal, "idle"), 20));
}

void test_board_remove_all() {
  NoticeBoard q;
  post(q, make("a"), 0);
  post(q, make("b"), 0);
  uint32_t v = q.version();
  TEST_ASSERT_EQUAL(2, q.remove_all());
  TEST_ASSERT_EQUAL(0, q.size());
  TEST_ASSERT_TRUE(q.version() != v);
  v = q.version();
  TEST_ASSERT_EQUAL(0, q.remove_all());
  TEST_ASSERT_EQUAL(v, q.version());
}

void test_board_priority_order() {
  NoticeBoard q;
  post(q, make("a", NoticePriority::Low), 0);
  post(q, make("b", NoticePriority::Normal), 10);
  post(q, make("c", NoticePriority::Low), 20);
  post(q, make("d", NoticePriority::High), 30);
  TEST_ASSERT_EQUAL_STRING("d", q.at(0).id);
  TEST_ASSERT_EQUAL_STRING("b", q.at(1).id);
  TEST_ASSERT_EQUAL_STRING("a", q.at(2).id);
  TEST_ASSERT_EQUAL_STRING("c", q.at(3).id);
}

void test_board_full_keeps_most_important() {
  NoticeBoard q;
  post(q, make("a", NoticePriority::Normal), 0);
  post(q, make("b"), 1);
  post(q, make("c"), 2);
  post(q, make("d"), 3);
  TEST_ASSERT_EQUAL(PostResult::Dropped, post(q, make("e"), 4));  // cheio e não mais importante
  TEST_ASSERT_EQUAL(PostResult::Added, post(q, make("f", NoticePriority::High), 5));  // sai o último
  TEST_ASSERT_EQUAL(4, q.size());
  TEST_ASSERT_EQUAL_STRING("f", q.at(0).id);
  TEST_ASSERT_EQUAL_STRING("c", q.at(3).id);
}

void test_board_dedup() {
  NoticeBoard q;
  TEST_ASSERT_EQUAL(PostResult::Added, post(q, make("a", NoticePriority::High, "", "x"), 1000));
  TEST_ASSERT_EQUAL(PostResult::Repeated, post(q, make("a", NoticePriority::High, "", "x"), 2000));
  TEST_ASSERT_EQUAL(PostResult::Added, post(q, make("a", NoticePriority::High, "", "y"), 2100));  // outro título
  TEST_ASSERT_EQUAL(PostResult::Added, post(q, make("a", NoticePriority::High, "", "y"), 5200));  // passou o tempo
  q.dismiss(0);
  TEST_ASSERT_EQUAL(PostResult::Repeated, post(q, make("a", NoticePriority::High, "", "y"), 6000));  // fechado, mas repetido
}

void test_board_expire() {
  NoticeBoard q;
  post(q, make("a", NoticePriority::Low, "", "t", 5), 1000);
  post(q, make("b"), 1000);
  TEST_ASSERT_EQUAL(0, q.expire(5999));
  TEST_ASSERT_EQUAL(1, q.expire(6000));
  TEST_ASSERT_EQUAL(1, q.size());
  TEST_ASSERT_EQUAL_STRING("b", q.at(0).id);
  // Volta do millis(): conta pela diferença.
  NoticeBoard w;
  post(w, make("a", NoticePriority::Low, "", "t", 2), 0xFFFFFF00u);
  TEST_ASSERT_EQUAL(0, w.expire(0x00000100u));   // 512 ms depois
  TEST_ASSERT_EQUAL(1, w.expire(0x00000800u));   // 2304 ms depois
}


// ---------- Ações e resposta (15c) ----------

void test_json_actions_list_and_status_fields() {
  NoticeFields f = parse(R"({"title":"t","actions":["allow", 3, {"x":"y"}, "deny"],"text":"5h",
    "text_color":"#93A0AD","bar":37,"ttl":600})", 4);
  TEST_ASSERT_EQUAL_STRING("allow,deny", f.actions);
  TEST_ASSERT_EQUAL_STRING("5h", f.text);
  TEST_ASSERT_EQUAL_STRING("#93A0AD", f.text_color);
  TEST_ASSERT_TRUE(f.has_bar && f.has_ttl);
  TEST_ASSERT_EQUAL(37, f.bar);
  TEST_ASSERT_EQUAL(600, f.ttl);
  // Uma string solta vale; lista em outra chave não entra.
  TEST_ASSERT_EQUAL_STRING("allow", parse(R"({"actions":"allow"})").actions);
  TEST_ASSERT_EQUAL_STRING("", parse(R"({"other":["allow"]})").actions);
  TEST_ASSERT_TRUE(parse(R"({"bar":"10"})").bar_bad);
  TEST_ASSERT_TRUE(parse(R"({"ttl":[1]})").ttl_bad);
}

void test_notice_actions() {
  Notice n;
  TEST_ASSERT_EQUAL(NoticeError::None, build(R"({"title":"t","actions":["deny","allow","later"]})", &n));
  TEST_ASSERT_EQUAL(NOTICE_ACTION_ALLOW | NOTICE_ACTION_DENY, n.actions);
  TEST_ASSERT_EQUAL(NoticeError::None, build(R"({"title":"t"})", &n));
  TEST_ASSERT_EQUAL(0, n.actions);
  TEST_ASSERT_EQUAL(NoticeError::None, build(R"({"title":"t","icon":"terminal"})", &n));
  TEST_ASSERT_EQUAL(NoticeIcon::Terminal, n.icon);
}

static Notice ask(const char *id, const char *title = "Rodar git", uint32_t timeout_s = 0) {
  Notice n = make(id, NoticePriority::High, "permission", title, timeout_s);
  n.actions = NOTICE_ACTION_ALLOW | NOTICE_ACTION_DENY;
  return n;
}

void test_board_answer_flow() {
  NoticeBoard q;
  Notice a = ask("s1");
  uint32_t rev = 0;
  TEST_ASSERT_EQUAL(PostResult::Added, q.post(a, 1000, &rev));
  TEST_ASSERT_TRUE(rev != 0);
  TEST_ASSERT_EQUAL(rev, q.rev(0));
  TEST_ASSERT_EQUAL(NoticeAnswer::None, q.answered("s1", 1000, nullptr));
  // Revisão errada não vale.
  TEST_ASSERT_FALSE(q.answer("s1", rev + 1, NoticeAnswer::Allow, 1500));
  TEST_ASSERT_TRUE(q.answer("s1", rev, NoticeAnswer::Allow, 1500));
  TEST_ASSERT_EQUAL(0, q.size());  // respondeu: o balão some
  uint32_t got = 0;
  TEST_ASSERT_EQUAL(NoticeAnswer::Allow, q.answered("s1", 2000, &got));
  TEST_ASSERT_EQUAL(rev, got);
  // Já respondido: não responde de novo.
  TEST_ASSERT_FALSE(q.answer("s1", rev, NoticeAnswer::Deny, 2000));
  // Guardada só por um tempo.
  TEST_ASSERT_EQUAL(NoticeAnswer::None, q.answered("s1", 1500 + NOTICE_ANSWER_KEEP_MS, nullptr));
}

void test_board_answer_guards() {
  NoticeBoard q;
  uint32_t rev = 0;
  // Sem ações: não aceita resposta.
  Notice plain = make("s1", NoticePriority::High, "permission", "x");
  q.post(plain, 0, &rev);
  TEST_ASSERT_FALSE(q.answer("s1", rev, NoticeAnswer::Allow, 10));
  // Só "deny": allow recusado.
  Notice d = ask("s2");
  d.actions = NOTICE_ACTION_DENY;
  q.post(d, 0, &rev);
  TEST_ASSERT_FALSE(q.answer("s2", rev, NoticeAnswer::Allow, 10));
  TEST_ASSERT_TRUE(q.answer("s2", rev, NoticeAnswer::Deny, 10));
  // Passou do timeout (antes do expire rodar): recusado.
  Notice t = ask("s3", "Rodar git", 5);
  q.post(t, 1000, &rev);
  TEST_ASSERT_FALSE(q.answer("s3", rev, NoticeAnswer::Allow, 6000));
  TEST_ASSERT_FALSE(q.answer("nada", 1, NoticeAnswer::Allow, 10));
}

void test_board_answer_new_request_forgets_old() {
  NoticeBoard q;
  uint32_t r1 = 0, r2 = 0;
  Notice a = ask("s1");
  q.post(a, 0, &r1);
  TEST_ASSERT_TRUE(q.answer("s1", r1, NoticeAnswer::Allow, 100));
  // O pedido seguinte do mesmo chat (mesmo título, logo depois) é novo, não
  // repetido, e a resposta antiga não vale pra ele.
  Notice b = ask("s1");
  TEST_ASSERT_EQUAL(PostResult::Added, q.post(b, 200, &r2));
  TEST_ASSERT_TRUE(r2 != r1);
  TEST_ASSERT_EQUAL(NoticeAnswer::None, q.answered("s1", 300, nullptr));
  // Dois pedidos abertos seguidos com o mesmo título: o segundo troca a revisão.
  uint32_t r3 = 0;
  Notice c = ask("s1");
  TEST_ASSERT_EQUAL(PostResult::Added, q.post(c, 400, &r3));
  TEST_ASSERT_TRUE(r3 != r2);
  TEST_ASSERT_FALSE(q.answer("s1", r2, NoticeAnswer::Allow, 500));
  TEST_ASSERT_EQUAL(1, q.size());
}

// ---------- StatusBoard (14b) ----------

static StatusError status(const std::string &body, const char *id, StatusItem *out) {
  NoticeFields f = parse(body);
  return status_from_fields(f, id, out);
}

void test_status_fields_and_defaults() {
  StatusItem it;
  TEST_ASSERT_EQUAL(StatusError::None, status(R"({"text":"2","icon":"terminal"})", "claude", &it));
  TEST_ASSERT_EQUAL_STRING("claude", it.id);
  TEST_ASSERT_EQUAL_STRING("2", it.text);
  TEST_ASSERT_TRUE(it.has_icon);
  TEST_ASSERT_EQUAL(NoticeIcon::Terminal, it.icon);
  TEST_ASSERT_EQUAL_HEX32(0x93A0AD, it.color);
  TEST_ASSERT_EQUAL_HEX32(0xECEFF2, it.text_color);
  TEST_ASSERT_EQUAL(-1, it.bar);
  TEST_ASSERT_EQUAL(0, it.ttl_s);
  TEST_ASSERT_EQUAL(StatusError::None,
                    status(R"({"text":"5h","icon":"rocket","color":"#6FCF6A","bar":37,"ttl":60})", "quota", &it));
  TEST_ASSERT_FALSE(it.has_icon);  // desconhecido: sem ícone
  TEST_ASSERT_EQUAL(37, it.bar);
  TEST_ASSERT_EQUAL_HEX32(0x6FCF6A, it.color);
  TEST_ASSERT_EQUAL(60, it.ttl_s);
}

void test_status_errors() {
  StatusItem it;
  TEST_ASSERT_EQUAL(StatusError::Text, status(R"({})", "a", &it));
  TEST_ASSERT_EQUAL(StatusError::Id, status(R"({"text":"x"})", "", &it));
  TEST_ASSERT_EQUAL(StatusError::Id, status(R"({"text":"x"})", "12345678901234567", &it));  // 17
  TEST_ASSERT_EQUAL(StatusError::Bar, status(R"({"text":"x","bar":101})", "a", &it));
  TEST_ASSERT_EQUAL(StatusError::Bar, status(R"({"text":"x","bar":-1})", "a", &it));
  TEST_ASSERT_EQUAL(StatusError::Ttl, status(R"({"text":"x","ttl":86401})", "a", &it));
  TEST_ASSERT_EQUAL(StatusError::Color, status(R"({"text":"x","color":"red"})", "a", &it));
  TEST_ASSERT_EQUAL(StatusError::TextColor, status(R"({"text":"x","text_color":"#12"})", "a", &it));
  TEST_ASSERT_EQUAL_STRING("text_color", status_error_field(StatusError::TextColor));
}

static StatusItem chip(const char *id, const char *text, uint32_t ttl = 0) {
  StatusItem it;
  memset(&it, 0, sizeof(it));
  snprintf(it.id, sizeof(it.id), "%s", id);
  snprintf(it.text, sizeof(it.text), "%s", text);
  it.bar = -1;
  it.ttl_s = ttl;
  return it;
}

void test_status_board() {
  StatusBoard b;
  TEST_ASSERT_EQUAL(StatusError::None, b.put(chip("claude", "1"), 0));
  TEST_ASSERT_EQUAL(StatusError::None, b.put(chip("quota", "5h"), 0));
  uint32_t v = b.version();
  // Mesmo conteúdo: não muda a versão (a Vitrine não redesenha).
  b.put(chip("claude", "1"), 10);
  TEST_ASSERT_EQUAL(v, b.version());
  // Mudou: troca no lugar, mantém a ordem.
  b.put(chip("claude", "2"), 20);
  TEST_ASSERT_TRUE(b.version() != v);
  TEST_ASSERT_EQUAL_STRING("claude", b.at(0).id);
  TEST_ASSERT_EQUAL_STRING("2", b.at(0).text);
  TEST_ASSERT_EQUAL(StatusError::None, b.put(chip("ci", "ok"), 0));
  TEST_ASSERT_EQUAL(StatusError::Full, b.put(chip("x", "y"), 0));
  TEST_ASSERT_EQUAL(1, b.remove("ci"));
  TEST_ASSERT_EQUAL(0, b.remove("ci"));
  TEST_ASSERT_EQUAL(2, b.size());
  TEST_ASSERT_EQUAL(2, b.remove_all());
}

void test_status_ttl_renews_on_put() {
  StatusBoard b;
  b.put(chip("claude", "1", 10), 0);
  b.put(chip("claude", "1", 10), 8000);  // mesmo estado: só renova o prazo
  TEST_ASSERT_EQUAL(0, b.expire(12000));
  TEST_ASSERT_EQUAL(1, b.expire(18000));
  TEST_ASSERT_EQUAL(0, b.size());
}

// ---------- HttpRequest ----------

struct Sink {
  std::string body;
};
static void sink(void *ctx, const char *d, size_t n) { ((Sink *)ctx)->body.append(d, n); }

void test_http_post_with_body() {
  std::string req =
      "POST /notices?x=1 HTTP/1.1\r\nHost: cydex.local\r\nAuthorization: Bearer a1b2c3d4\r\n"
      "content-type: application/json\r\nContent-Length: 11\r\n\r\n{\"a\":\"bcd\"}EXTRA";
  HttpRequest r;
  Sink s;
  size_t used = 0;
  for (size_t i = 0; i < req.size() && !r.done(); i += 5) {
    used += r.feed(req.data() + i, std::min<size_t>(5, req.size() - i), sink, &s);
  }
  TEST_ASSERT_TRUE(r.done());
  TEST_ASSERT_EQUAL_STRING("POST", r.method());
  TEST_ASSERT_EQUAL_STRING("/notices", r.path());
  TEST_ASSERT_EQUAL_STRING("x=1", r.query());
  TEST_ASSERT_EQUAL_STRING("a1b2c3d4", r.bearer());
  TEST_ASSERT_EQUAL(11, r.content_length());
  TEST_ASSERT_EQUAL_STRING("{\"a\":\"bcd\"}", s.body.c_str());
  TEST_ASSERT_EQUAL(req.size() - 5, used);
}

void test_http_get_no_body_and_long_header() {
  std::string req = "GET / HTTP/1.1\nCookie: " + std::string(400, 'c') + "\nX: y\n\n";
  HttpRequest r;
  r.feed(req.data(), req.size(), nullptr, nullptr);
  TEST_ASSERT_TRUE(r.done());
  TEST_ASSERT_EQUAL_STRING("GET", r.method());
  TEST_ASSERT_EQUAL_STRING("/", r.path());
  TEST_ASSERT_EQUAL(-1, r.content_length());
  TEST_ASSERT_EQUAL_STRING("", r.bearer());
}

void test_http_delete_with_query() {
  std::string req = "DELETE /notices/2f1c9a4e-7b3d-4c1a-9e8f-0123456789ab?tag=permission HTTP/1.1\r\n\r\n";
  HttpRequest r;
  r.feed(req.data(), req.size(), nullptr, nullptr);
  TEST_ASSERT_TRUE(r.done());
  TEST_ASSERT_EQUAL_STRING("DELETE", r.method());
  TEST_ASSERT_EQUAL_STRING("/notices/2f1c9a4e-7b3d-4c1a-9e8f-0123456789ab", r.path());
  char tag[17];
  TEST_ASSERT_TRUE(http_query_param(r.query(), "tag", tag, sizeof(tag)));
  TEST_ASSERT_EQUAL_STRING("permission", tag);

  std::string longp = "GET /saves/" + std::string(130, 'a') + " HTTP/1.1\r\n\r\n";
  r.reset();
  r.feed(longp.data(), longp.size(), nullptr, nullptr);
  TEST_ASSERT_TRUE(r.failed());
}

void test_http_query_param() {
  char v[8];
  TEST_ASSERT_TRUE(http_query_param("a=1&tag=x%2Dy+z", "tag", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("x-y z", v);
  TEST_ASSERT_FALSE(http_query_param("atag=1&ta=2", "tag", v, sizeof(v)));
  TEST_ASSERT_TRUE(http_query_param("tag=", "tag", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("", v);
  TEST_ASSERT_FALSE(http_query_param("tag=123456789", "tag", v, sizeof(v)));  // não cabe
  TEST_ASSERT_FALSE(http_query_param("", "tag", v, sizeof(v)));
}

void test_http_garbage() {
  HttpRequest r;
  r.feed("\r\n", 2, nullptr, nullptr);
  TEST_ASSERT_TRUE(r.failed());
  r.reset();
  const char *bad = "POST /h HTTP/1.1\r\nContent-Length: abc\r\n\r\n";
  r.feed(bad, strlen(bad), nullptr, nullptr);
  TEST_ASSERT_TRUE(r.failed());
}

// ---------- WifiList ----------

void test_wifi_list_dedup_sort_and_cap() {
  WifiList l;
  l.add("Casa", -70, true);
  l.add("", -30, true);  // oculta
  l.add("Casa", -50, true);
  l.add("Vizinho", -60, true);
  l.add("Aberta", -80, false);
  TEST_ASSERT_EQUAL(3, l.count);
  TEST_ASSERT_EQUAL(-50, l.items[l.find("Casa")].rssi);
  l.sort(nullptr);
  TEST_ASSERT_EQUAL_STRING("Casa", l.items[0].ssid);
  TEST_ASSERT_EQUAL_STRING("Aberta", l.items[2].ssid);
  l.sort("Aberta");
  TEST_ASSERT_EQUAL_STRING("Aberta", l.items[0].ssid);
  TEST_ASSERT_EQUAL_STRING("Casa", l.items[1].ssid);

  WifiList full;
  char name[8];
  for (int i = 0; i < WIFI_LIST_MAX; i++) {
    snprintf(name, sizeof(name), "n%d", i);
    full.add(name, (int8_t)(-90 + i), true);
  }
  full.add("forte", -40, true);  // troca a mais fraca (n0)
  full.add("fraca", -95, true);  // não entra
  TEST_ASSERT_EQUAL(WIFI_LIST_MAX, full.count);
  TEST_ASSERT_TRUE(full.find("forte") >= 0);
  TEST_ASSERT_EQUAL(-1, full.find("n0"));
  TEST_ASSERT_EQUAL(-1, full.find("fraca"));
}

void test_wifi_bars() {
  TEST_ASSERT_EQUAL(4, wifi_bars(-40));
  TEST_ASSERT_EQUAL(3, wifi_bars(-60));
  TEST_ASSERT_EQUAL(2, wifi_bars(-75));
  TEST_ASSERT_EQUAL(1, wifi_bars(-90));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fit_collapses_whitespace_and_markdown);
  RUN_TEST(test_fit_keeps_latin1_and_substitutes);
  RUN_TEST(test_fit_drops_emoji_and_cjk);
  RUN_TEST(test_fit_truncates_with_ellipsis_on_char_boundary);
  RUN_TEST(test_fit_exact_fit_has_no_ellipsis);
  RUN_TEST(test_fit_null_and_empty);
  RUN_TEST(test_json_all_fields);
  RUN_TEST(test_json_skips_unknown_and_nested);
  RUN_TEST(test_json_scalar_at_end_and_spaces);
  RUN_TEST(test_json_bad_scalars);
  RUN_TEST(test_json_escapes_and_unicode);
  RUN_TEST(test_json_long_value_keeps_beginning);
  RUN_TEST(test_json_cut_never_splits_utf8);
  RUN_TEST(test_json_invalid);
  RUN_TEST(test_notice_defaults_info);
  RUN_TEST(test_notice_presets_and_overrides);
  RUN_TEST(test_notice_text_is_fitted);
  RUN_TEST(test_notice_errors);
  RUN_TEST(test_board_auto_id);
  RUN_TEST(test_board_replace_by_id);
  RUN_TEST(test_board_remove_with_tag);
  RUN_TEST(test_board_remove_all);
  RUN_TEST(test_board_priority_order);
  RUN_TEST(test_board_full_keeps_most_important);
  RUN_TEST(test_board_dedup);
  RUN_TEST(test_board_expire);
  RUN_TEST(test_json_actions_list_and_status_fields);
  RUN_TEST(test_notice_actions);
  RUN_TEST(test_board_answer_flow);
  RUN_TEST(test_board_answer_guards);
  RUN_TEST(test_board_answer_new_request_forgets_old);
  RUN_TEST(test_status_fields_and_defaults);
  RUN_TEST(test_status_errors);
  RUN_TEST(test_status_board);
  RUN_TEST(test_status_ttl_renews_on_put);
  RUN_TEST(test_http_post_with_body);
  RUN_TEST(test_http_get_no_body_and_long_header);
  RUN_TEST(test_http_delete_with_query);
  RUN_TEST(test_http_query_param);
  RUN_TEST(test_http_garbage);
  RUN_TEST(test_wifi_list_dedup_sort_and_cap);
  RUN_TEST(test_wifi_bars);
  return UNITY_END();
}
