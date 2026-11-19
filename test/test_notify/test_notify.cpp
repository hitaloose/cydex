#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "logic/TextFit.h"
#include "net/HttpRequest.h"
#include "net/WifiList.h"
#include "notify/HookJson.h"
#include "notify/Notice.h"

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

// ---------- HookJson ----------

static HookFields parse(const std::string &body, size_t chunk = 7) {
  HookJson j;
  for (size_t i = 0; i < body.size(); i += chunk) {
    j.feed(body.data() + i, std::min(chunk, body.size() - i));
  }
  TEST_ASSERT_TRUE_MESSAGE(j.done(), "JSON não terminou");
  return j.fields();
}

void test_json_notification_fields() {
  HookFields f = parse(R"({"session_id":"abc123","transcript_path":"/x.jsonl","cwd":"/Users/me/dev/cydex",
    "hook_event_name":"Notification","message":"Claude needs your permission to use Bash",
    "notification_type":"permission_prompt"})");
  TEST_ASSERT_EQUAL_STRING("Notification", f.event);
  TEST_ASSERT_EQUAL_STRING("permission_prompt", f.type);
  TEST_ASSERT_EQUAL_STRING("Claude needs your permission to use Bash", f.message);
  TEST_ASSERT_EQUAL_STRING("/Users/me/dev/cydex", f.cwd);
}

void test_json_skips_nested_and_scalars() {
  HookFields f = parse(R"({"stop_hook_active":true,"n":-12.5e3,"x":null,
    "background_tasks":[{"id":"t","command":"echo \"}]\""}],"session_crons":[],
    "hook_event_name":"Stop","last_assistant_message":"Done."})", 3);
  TEST_ASSERT_EQUAL_STRING("Stop", f.event);
  TEST_ASSERT_EQUAL_STRING("Done.", f.last);
}

void test_json_escapes_and_unicode() {
  HookFields f = parse(R"({"hook_event_name":"Stop","last_assistant_message":"a\"b\\c\nd \u00e9 \ud83c\udf89"})");
  TEST_ASSERT_EQUAL_STRING("a\"b\\c\nd é \xF0\x9F\x8E\x89", f.last);
}

void test_json_long_value_keeps_beginning() {
  std::string big(50000, 'x');
  HookFields f = parse("{\"hook_event_name\":\"Stop\",\"last_assistant_message\":\"start" + big +
                       "\",\"cwd\":\"/a/b\"}", 997);
  TEST_ASSERT_EQUAL(sizeof(f.last) - 1, strlen(f.last));
  TEST_ASSERT_EQUAL(0, strncmp(f.last, "startxxx", 8));
  TEST_ASSERT_EQUAL_STRING("/a/b", f.cwd);
}

void test_json_long_cwd_keeps_end() {
  std::string deep;
  for (int i = 0; i < 30; i++) deep += "/pasta" + std::to_string(i);
  HookFields f = parse("{\"cwd\":\"" + deep + "/meu-projeto\",\"hook_event_name\":\"Stop\"}");
  size_t n = strlen(f.cwd);
  TEST_ASSERT_TRUE(n < sizeof(f.cwd));
  TEST_ASSERT_EQUAL_STRING("/meu-projeto", f.cwd + n - strlen("/meu-projeto"));
}

void test_json_invalid() {
  HookJson j;
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

static HookFields fields(const char *event, const char *type, const char *msg, const char *last,
                         const char *cwd, const char *session = "") {
  HookFields f;
  memset(&f, 0, sizeof(f));
  snprintf(f.event, sizeof(f.event), "%s", event);
  snprintf(f.type, sizeof(f.type), "%s", type);
  snprintf(f.message, sizeof(f.message), "%s", msg);
  snprintf(f.last, sizeof(f.last), "%s", last);
  snprintf(f.cwd, sizeof(f.cwd), "%s", cwd);
  snprintf(f.session, sizeof(f.session), "%s", session);
  return f;
}

static bool notice_from_hook(const HookFields &f, Notice *n) {
  NoticeClear c;
  return hook_action(f, n, &c) == HookAction::Show;
}

void test_notice_kinds() {
  Notice n;
  TEST_ASSERT_TRUE(notice_from_hook(
      fields("Notification", "permission_prompt", "Needs permission", "", "/a/cydex"), &n));
  TEST_ASSERT_EQUAL(NoticeKind::Permission, n.kind);
  TEST_ASSERT_EQUAL_STRING("cydex", n.project);
  TEST_ASSERT_EQUAL_STRING("Needs permission", n.message);

  TEST_ASSERT_TRUE(notice_from_hook(fields("Notification", "idle_prompt", "Waiting", "", "/a/b/"), &n));
  TEST_ASSERT_EQUAL(NoticeKind::Idle, n.kind);
  TEST_ASSERT_EQUAL_STRING("b", n.project);

  TEST_ASSERT_TRUE(notice_from_hook(fields("Stop", "", "", "**All** done.\nNext", "C:\\dev\\app"), &n));
  TEST_ASSERT_EQUAL(NoticeKind::Done, n.kind);
  TEST_ASSERT_EQUAL_STRING("app", n.project);
  TEST_ASSERT_EQUAL_STRING("All done. Next", n.message);
}

void test_notice_ignored_events() {
  Notice n;
  TEST_ASSERT_FALSE(notice_from_hook(fields("Notification", "auth_success", "x", "", "/a"), &n));
  TEST_ASSERT_FALSE(notice_from_hook(fields("PreToolUse", "", "", "", "/a"), &n));
  TEST_ASSERT_FALSE(notice_from_hook(fields("", "", "", "", ""), &n));
}

void test_notice_root_cwd() {
  Notice n;
  TEST_ASSERT_TRUE(notice_from_hook(fields("Stop", "", "", "", "/"), &n));
  TEST_ASSERT_EQUAL_STRING("/", n.project);
}

static Notice make(NoticeKind k, const char *project, const char *session = "") {
  Notice n;
  memset(&n, 0, sizeof(n));
  n.kind = k;
  snprintf(n.project, sizeof(n.project), "%s", project);
  snprintf(n.session, sizeof(n.session), "%s", session);
  return n;
}

void test_json_session_id() {
  HookFields f = parse(R"({"session_id":"2f1c9a4e-7b3d-4c1a-9e8f-0123456789ab","hook_event_name":"UserPromptSubmit","prompt":"segredo"})");
  TEST_ASSERT_EQUAL_STRING("2f1c9a4e-7b3d-4c1a-9e8f-0123456789ab", f.session);
  TEST_ASSERT_EQUAL_STRING("UserPromptSubmit", f.event);
}

void test_hook_action_clear_events() {
  Notice n;
  NoticeClear c;
  TEST_ASSERT_EQUAL(HookAction::Clear, hook_action(fields("UserPromptSubmit", "", "", "", "/a", "s1"), &n, &c));
  TEST_ASSERT_EQUAL(NOTICE_ALL, c.kinds);
  TEST_ASSERT_EQUAL_STRING("s1", c.session);
  TEST_ASSERT_EQUAL(HookAction::Clear, hook_action(fields("SessionEnd", "", "", "", "/a", "s1"), &n, &c));
  TEST_ASSERT_EQUAL(NOTICE_ALL, c.kinds);
  for (const char *ev : {"PostToolBatch", "PostToolUse", "PostToolUseFailure"}) {
    TEST_ASSERT_EQUAL(HookAction::Clear, hook_action(fields(ev, "", "", "", "/a", "s2"), &n, &c));
    TEST_ASSERT_EQUAL(notice_bit(NoticeKind::Permission), c.kinds);
  }
  TEST_ASSERT_EQUAL(HookAction::Ignore, hook_action(fields("PreToolUse", "", "", "", "/a", "s"), &n, &c));
  TEST_ASSERT_EQUAL(HookAction::Show,
                    hook_action(fields("Notification", "idle_prompt", "w", "", "/a", "s3"), &n, &c));
  TEST_ASSERT_EQUAL_STRING("s3", n.session);
}

void test_notice_matches_session_and_kind() {
  NoticeClear perm{"s1", notice_bit(NoticeKind::Permission)};
  TEST_ASSERT_TRUE(notice_matches(make(NoticeKind::Permission, "a", "s1"), perm));
  TEST_ASSERT_FALSE(notice_matches(make(NoticeKind::Permission, "a", "s2"), perm));  // outra sessão
  TEST_ASSERT_FALSE(notice_matches(make(NoticeKind::Idle, "a", "s1"), perm));        // outro tipo
  TEST_ASSERT_TRUE(notice_matches(make(NoticeKind::Permission, "a", ""), perm));     // sem sessão
  NoticeClear any{"", NOTICE_ALL};
  TEST_ASSERT_TRUE(notice_matches(make(NoticeKind::Done, "a", "s9"), any));
}

void test_board_remove_and_resets_dedup() {
  NoticeBoard q;
  q.post(make(NoticeKind::Permission, "a", "s1"), 0);
  q.post(make(NoticeKind::Idle, "b", "s2"), 10);
  q.post(make(NoticeKind::Done, "c", "s3"), 30);
  TEST_ASSERT_EQUAL(1, q.remove(NoticeClear{"s1", NOTICE_ALL}));
  TEST_ASSERT_EQUAL(2, q.size());
  TEST_ASSERT_EQUAL_STRING("b", q.at(0).project);
  TEST_ASSERT_EQUAL_STRING("c", q.at(1).project);
  // Só Permission da s2: o Idle fica.
  TEST_ASSERT_EQUAL(0, q.remove(NoticeClear{"s2", notice_bit(NoticeKind::Permission)}));
  // Depois de limpar, o mesmo aviso logo em seguida entra de novo.
  TEST_ASSERT_TRUE(q.post(make(NoticeKind::Done, "c", "s3"), 40));
  TEST_ASSERT_EQUAL(1, q.remove(NoticeClear{"s3", NOTICE_ALL}));
  TEST_ASSERT_TRUE(q.post(make(NoticeKind::Done, "c", "s3"), 41));
}

void test_board_priority_order() {
  NoticeBoard q;
  q.post(make(NoticeKind::Done, "a", "s1"), 0);
  q.post(make(NoticeKind::Idle, "b", "s2"), 10);
  q.post(make(NoticeKind::Done, "c", "s3"), 20);
  q.post(make(NoticeKind::Permission, "d", "s4"), 30);
  TEST_ASSERT_EQUAL(4, q.size());
  TEST_ASSERT_EQUAL_STRING("d", q.at(0).project);  // permissão na frente
  TEST_ASSERT_EQUAL_STRING("b", q.at(1).project);  // esperando
  TEST_ASSERT_EQUAL_STRING("a", q.at(2).project);  // terminou, o mais antigo antes
  TEST_ASSERT_EQUAL_STRING("c", q.at(3).project);
  q.dismiss(0);
  TEST_ASSERT_EQUAL_STRING("b", q.at(0).project);
}

void test_board_one_per_chat() {
  NoticeBoard q;
  // Dois Claude no mesmo projeto: um aviso cada.
  q.post(make(NoticeKind::Permission, "cydex", "s1"), 0);
  q.post(make(NoticeKind::Done, "cydex", "s2"), 10);
  TEST_ASSERT_EQUAL(2, q.size());
  // O mesmo chat de novo: substitui (é o estado atual dele).
  TEST_ASSERT_TRUE(q.post(make(NoticeKind::Done, "cydex", "s1"), 20));
  TEST_ASSERT_EQUAL(2, q.size());
  TEST_ASSERT_EQUAL(NoticeKind::Done, q.at(0).kind);
  TEST_ASSERT_EQUAL(NoticeKind::Done, q.at(1).kind);
  TEST_ASSERT_EQUAL_STRING("s2", q.at(0).session);  // o mais antigo antes
  // Sem sessão: a chave é o projeto.
  q.post(make(NoticeKind::Idle, "x"), 30);
  TEST_ASSERT_TRUE(q.post(make(NoticeKind::Permission, "x"), 40));
  TEST_ASSERT_EQUAL(3, q.size());
  TEST_ASSERT_EQUAL_STRING("x", q.at(0).project);
}

void test_board_full_keeps_most_important() {
  NoticeBoard q;
  q.post(make(NoticeKind::Idle, "a", "s1"), 0);
  q.post(make(NoticeKind::Done, "b", "s2"), 1);
  q.post(make(NoticeKind::Done, "c", "s3"), 2);
  q.post(make(NoticeKind::Done, "d", "s4"), 3);
  TEST_ASSERT_FALSE(q.post(make(NoticeKind::Done, "e", "s5"), 4));  // cheio e menos importante
  TEST_ASSERT_TRUE(q.post(make(NoticeKind::Permission, "f", "s6"), 5));  // sai o último Done
  TEST_ASSERT_EQUAL(4, q.size());
  TEST_ASSERT_EQUAL_STRING("f", q.at(0).project);
  TEST_ASSERT_EQUAL_STRING("c", q.at(3).project);
}

void test_board_dedup() {
  NoticeBoard q;
  TEST_ASSERT_TRUE(q.post(make(NoticeKind::Permission, "a"), 1000));
  TEST_ASSERT_FALSE(q.post(make(NoticeKind::Permission, "a"), 2000));  // repetido
  TEST_ASSERT_EQUAL(1, q.size());
  TEST_ASSERT_TRUE(q.post(make(NoticeKind::Permission, "a"), 5000));  // passou o tempo
  TEST_ASSERT_EQUAL(1, q.size());
  q.dismiss(0);
  TEST_ASSERT_FALSE(q.post(make(NoticeKind::Permission, "a"), 6000));  // fechado, mas repetido
}

// ---------- HttpRequest ----------

struct Sink {
  std::string body;
};
static void sink(void *ctx, const char *d, size_t n) { ((Sink *)ctx)->body.append(d, n); }

void test_http_post_with_body() {
  std::string req =
      "POST /hook?x=1 HTTP/1.1\r\nHost: cydex.local\r\nAuthorization: Bearer a1b2c3d4\r\n"
      "content-type: application/json\r\nContent-Length: 11\r\n\r\n{\"a\":\"bcd\"}EXTRA";
  HttpRequest r;
  Sink s;
  size_t used = 0;
  for (size_t i = 0; i < req.size() && !r.done(); i += 5) {
    used += r.feed(req.data() + i, std::min<size_t>(5, req.size() - i), sink, &s);
  }
  TEST_ASSERT_TRUE(r.done());
  TEST_ASSERT_EQUAL_STRING("POST", r.method());
  TEST_ASSERT_EQUAL_STRING("/hook", r.path());
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
  RUN_TEST(test_json_notification_fields);
  RUN_TEST(test_json_skips_nested_and_scalars);
  RUN_TEST(test_json_escapes_and_unicode);
  RUN_TEST(test_json_long_value_keeps_beginning);
  RUN_TEST(test_json_long_cwd_keeps_end);
  RUN_TEST(test_json_invalid);
  RUN_TEST(test_notice_kinds);
  RUN_TEST(test_notice_ignored_events);
  RUN_TEST(test_notice_root_cwd);
  RUN_TEST(test_board_dedup);
  RUN_TEST(test_json_session_id);
  RUN_TEST(test_hook_action_clear_events);
  RUN_TEST(test_notice_matches_session_and_kind);
  RUN_TEST(test_board_remove_and_resets_dedup);
  RUN_TEST(test_board_priority_order);
  RUN_TEST(test_board_one_per_chat);
  RUN_TEST(test_board_full_keeps_most_important);
  RUN_TEST(test_http_post_with_body);
  RUN_TEST(test_http_get_no_body_and_long_header);
  RUN_TEST(test_http_garbage);
  RUN_TEST(test_wifi_list_dedup_sort_and_cap);
  RUN_TEST(test_wifi_bars);
  return UNITY_END();
}
