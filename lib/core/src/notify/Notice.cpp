#include "Notice.h"

#include <cstring>

#include "logic/TextFit.h"

bool notice_matches(const Notice &n, const NoticeClear &c) {
  if (!(c.kinds & notice_bit(n.kind))) return false;
  return !c.session[0] || !n.session[0] || strcmp(c.session, n.session) == 0;
}

static bool notice_from_hook(const HookFields &f, Notice *out) {
  const char *msg;
  if (strcmp(f.event, "Notification") == 0) {
    if (strcmp(f.type, "permission_prompt") == 0) out->kind = NoticeKind::Permission;
    else if (strcmp(f.type, "idle_prompt") == 0) out->kind = NoticeKind::Idle;
    else return false;
    msg = f.message;
  } else if (strcmp(f.event, "Stop") == 0) {
    out->kind = NoticeKind::Done;
    msg = f.last;
  } else {
    return false;
  }

  // Nome da pasta: o que vem depois da última '/' ou '\' (ignorando as do fim).
  char cwd[sizeof(f.cwd)];
  memcpy(cwd, f.cwd, sizeof(cwd));
  cwd[sizeof(cwd) - 1] = '\0';
  size_t len = strlen(cwd);
  while (len > 1 && (cwd[len - 1] == '/' || cwd[len - 1] == '\\')) cwd[--len] = '\0';
  const char *base = cwd;
  for (const char *p = cwd; *p; p++) {
    if ((*p == '/' || *p == '\\') && p[1]) base = p + 1;
  }
  text_fit(base, out->project, sizeof(out->project));
  text_fit(msg, out->message, sizeof(out->message));
  memcpy(out->session, f.session, sizeof(out->session));
  out->session[sizeof(out->session) - 1] = '\0';
  return true;
}

HookAction hook_action(const HookFields &f, Notice *show, NoticeClear *clear) {
  if (notice_from_hook(f, show)) return HookAction::Show;
  uint8_t kinds = 0;
  if (strcmp(f.event, "UserPromptSubmit") == 0 || strcmp(f.event, "SessionEnd") == 0) {
    kinds = NOTICE_ALL;
  } else if (strcmp(f.event, "PostToolBatch") == 0 || strcmp(f.event, "PostToolUse") == 0 ||
             strcmp(f.event, "PostToolUseFailure") == 0) {
    kinds = notice_bit(NoticeKind::Permission);
  }
  if (!kinds) return HookAction::Ignore;
  clear->kinds = kinds;
  memcpy(clear->session, f.session, sizeof(clear->session));
  clear->session[sizeof(clear->session) - 1] = '\0';
  return HookAction::Clear;
}

bool notice_same_chat(const Notice &a, const Notice &b) {
  if (a.session[0] && b.session[0]) return strcmp(a.session, b.session) == 0;
  return strcmp(a.project, b.project) == 0;
}

void NoticeBoard::erase(uint8_t i) {
  for (uint8_t k = i; k + 1 < count_; k++) e_[k] = e_[k + 1];
  count_--;
}

void NoticeBoard::sort() {
  // Inserção: no máximo 4 itens.
  for (uint8_t i = 1; i < count_; i++) {
    Entry v = e_[i];
    int j = i - 1;
    while (j >= 0 && ((uint8_t)e_[j].n.kind > (uint8_t)v.n.kind ||
                      (e_[j].n.kind == v.n.kind && e_[j].seq > v.seq))) {
      e_[j + 1] = e_[j];
      j--;
    }
    e_[j + 1] = v;
  }
}

bool NoticeBoard::post(const Notice &n, uint32_t now_ms) {
  if (has_last_ && last_.kind == n.kind && notice_same_chat(last_, n) &&
      now_ms - last_ms_ < NOTICE_DEDUP_MS) {
    return false;
  }
  has_last_ = true;
  last_ = n;
  last_ms_ = now_ms;

  // Um por chat: sai o anterior do mesmo chat.
  for (uint8_t i = 0; i < count_; i++) {
    if (notice_same_chat(e_[i].n, n)) {
      erase(i);
      break;
    }
  }
  uint32_t seq = ++seq_;
  if (count_ == NOTICE_BOARD_CAP) {
    // Cheio: sai o menos importante (o último), se o novo for mais importante.
    // Empate: o novo é mais recente e perde (o mais antigo vai antes).
    if ((uint8_t)n.kind >= (uint8_t)e_[count_ - 1].n.kind) {
      version_++;  // o anterior do mesmo chat pode ter saído
      return false;
    }
    count_--;
  }
  e_[count_].n = n;
  e_[count_].seq = seq;
  count_++;
  sort();
  version_++;
  return true;
}

uint8_t NoticeBoard::remove(const NoticeClear &c) {
  uint8_t removed = 0;
  for (uint8_t i = 0; i < count_;) {
    if (notice_matches(e_[i].n, c)) {
      erase(i);
      removed++;
    } else {
      i++;
    }
  }
  if (removed) {
    has_last_ = false;  // respondeu: o mesmo aviso logo depois é novo
    version_++;
  }
  return removed;
}

void NoticeBoard::dismiss(uint8_t i) {
  if (i >= count_) return;
  erase(i);
  version_++;
}
