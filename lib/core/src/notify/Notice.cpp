#include "Notice.h"

#include <cstdio>
#include <cstring>

#include "logic/TextFit.h"

namespace {

struct Preset {
  const char *name;
  uint32_t color;
  NoticeIcon icon;
  NoticePriority priority;
};

// O primeiro é o padrão (sem `type`). Cores do tema (src/ui/theme.h).
const Preset kPresets[] = {
    {"info", 0x93A0AD, NoticeIcon::Info, NoticePriority::Low},        // muted
    {"success", 0x6FCF6A, NoticeIcon::Check, NoticePriority::Low},    // NOTICE_DONE
    {"warning", 0xFF8A4C, NoticeIcon::Alert, NoticePriority::Normal}, // = Fogo
    {"waiting", 0xF5D547, NoticeIcon::Clock, NoticePriority::Normal}, // NOTICE_IDLE
    {"question", 0xFF5A4E, NoticeIcon::Shield, NoticePriority::High}, // accent
    {"error", 0xFF5A4E, NoticeIcon::Alert, NoticePriority::High},     // accent
};

const char *const kIcons[] = {"info", "check", "alert", "shield", "clock", "bell", "terminal"};
const char *const kPriorities[] = {"high", "normal", "low"};

int index_of(const char *s, const char *const *names, int n) {
  for (int i = 0; i < n; i++)
    if (strcmp(s, names[i]) == 0) return i;
  return -1;
}

}  // namespace

static int hex(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

bool notice_parse_color(const char *s, uint32_t *out) {
  if (s[0] != '#' || strlen(s) != 7) return false;
  uint32_t v = 0;
  for (int i = 1; i < 7; i++) {
    int h = hex(s[i]);
    if (h < 0) return false;
    v = (v << 4) | (uint32_t)h;
  }
  *out = v;
  return true;
}


const char *notice_error_field(NoticeError e) {
  switch (e) {
    case NoticeError::Title: return "title";
    case NoticeError::Id: return "id";
    case NoticeError::Tag: return "tag";
    case NoticeError::Type: return "type";
    case NoticeError::Priority: return "priority";
    case NoticeError::Color: return "color";
    case NoticeError::Timeout: return "timeout";
    case NoticeError::Sound: return "sound";
    case NoticeError::Wake: return "wake";
    default: return "";
  }
}

bool notice_valid_id(const char *s, unsigned max_len) {
  unsigned n = 0;
  for (; s[n]; n++) {
    char ch = s[n];
    bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
              ch == '.' || ch == '_' || ch == ':' || ch == '-';
    if (!ok || n >= max_len) return false;
  }
  return true;
}

NoticeError notice_from_fields(const NoticeFields &f, Notice *out) {
  memset(out, 0, sizeof(*out));

  if (!notice_valid_id(f.id, sizeof(out->id) - 1)) return NoticeError::Id;
  if (!notice_valid_id(f.tag, sizeof(out->tag) - 1)) return NoticeError::Tag;
  memcpy(out->id, f.id, strlen(f.id) + 1);
  memcpy(out->tag, f.tag, strlen(f.tag) + 1);

  const Preset *p = &kPresets[0];
  if (f.type[0]) {
    p = nullptr;
    for (const Preset &k : kPresets)
      if (strcmp(f.type, k.name) == 0) p = &k;
    if (!p) return NoticeError::Type;
  }
  out->type = (uint8_t)(p - kPresets);
  out->color = p->color;
  out->icon = p->icon;
  out->priority = p->priority;

  if (f.priority[0]) {
    int i = index_of(f.priority, kPriorities, 3);
    if (i < 0) return NoticeError::Priority;
    out->priority = (NoticePriority)i;
  }
  if (f.color[0] && !notice_parse_color(f.color, &out->color)) return NoticeError::Color;
  if (f.icon[0]) notice_icon_from_name(f.icon, &out->icon);

  // "allow,deny": nomes desconhecidos ficam de fora (firmware antigo ignora
  // ação nova; quem manda vê pela falta da resposta).
  for (const char *p = f.actions; *p;) {
    const char *end = strchr(p, ',');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len == 5 && strncmp(p, "allow", 5) == 0) out->actions |= NOTICE_ACTION_ALLOW;
    else if (len == 4 && strncmp(p, "deny", 4) == 0) out->actions |= NOTICE_ACTION_DENY;
    p += len + (end ? 1 : 0);
  }

  if (f.timeout_bad || (f.has_timeout && (f.timeout < 0 || (uint32_t)f.timeout > NOTICE_MAX_TIMEOUT_S)))
    return NoticeError::Timeout;
  out->timeout_s = f.has_timeout ? (uint32_t)f.timeout : 0;
  if (f.sound_bad) return NoticeError::Sound;
  if (f.wake_bad) return NoticeError::Wake;
  out->sound = f.has_sound ? f.sound : true;
  out->wake = f.has_wake ? f.wake : true;

  text_fit(f.title, out->title, sizeof(out->title));
  if (!out->title[0]) return NoticeError::Title;
  text_fit(f.subtitle, out->subtitle, sizeof(out->subtitle));
  text_fit(f.message, out->message, sizeof(out->message));
  return NoticeError::None;
}

bool notice_icon_from_name(const char *s, NoticeIcon *out) {
  int i = index_of(s, kIcons, (int)(sizeof(kIcons) / sizeof(kIcons[0])));
  if (i < 0) return false;
  *out = (NoticeIcon)i;
  return true;
}

const char *notice_answer_name(NoticeAnswer a) {
  return a == NoticeAnswer::Allow ? "allow" : a == NoticeAnswer::Deny ? "deny" : "";
}

const char *notice_type_name(const Notice &n) {
  return n.type < sizeof(kPresets) / sizeof(kPresets[0]) ? kPresets[n.type].name : "info";
}

void NoticeBoard::changed() { version_++; }

void NoticeBoard::erase(uint8_t i) {
  for (uint8_t k = i; k + 1 < count_; k++) e_[k] = e_[k + 1];
  count_--;
}

void NoticeBoard::sort() {
  // Inserção: no máximo 4 itens.
  for (uint8_t i = 1; i < count_; i++) {
    Entry v = e_[i];
    int j = i - 1;
    while (j >= 0 && ((uint8_t)e_[j].n.priority > (uint8_t)v.n.priority ||
                      (e_[j].n.priority == v.n.priority && e_[j].seq > v.seq))) {
      e_[j + 1] = e_[j];
      j--;
    }
    e_[j + 1] = v;
  }
}

int NoticeBoard::find(const char *id) const {
  for (uint8_t i = 0; i < count_; i++)
    if (strcmp(e_[i].n.id, id) == 0) return i;
  return -1;
}

void NoticeBoard::forget_answer(const char *id) {
  for (Answer &a : ans_)
    if (a.a != NoticeAnswer::None && strcmp(a.id, id) == 0) a.a = NoticeAnswer::None;
}

bool NoticeBoard::answer(const char *id, uint32_t rev, NoticeAnswer a, uint32_t now_ms) {
  int i = find(id);
  if (i < 0 || a == NoticeAnswer::None || e_[i].seq != rev) return false;
  const Notice &n = e_[i].n;
  uint8_t need = a == NoticeAnswer::Allow ? NOTICE_ACTION_ALLOW : NOTICE_ACTION_DENY;
  if (!(n.actions & need)) return false;
  if (n.timeout_s && now_ms - e_[i].posted_ms >= n.timeout_s * 1000u) return false;
  forget_answer(id);
  Answer &slot = ans_[ans_next_];
  ans_next_ = (uint8_t)((ans_next_ + 1) % NOTICE_BOARD_CAP);
  memcpy(slot.id, n.id, sizeof(slot.id));
  slot.rev = rev;
  slot.ms = now_ms;
  slot.a = a;
  erase((uint8_t)i);
  // Respondeu: o mesmo pedido logo depois é novo, não repetido.
  if (strcmp(last_id_, id) == 0) has_last_ = false;
  changed();
  return true;
}

NoticeAnswer NoticeBoard::answered(const char *id, uint32_t now_ms, uint32_t *rev) const {
  for (const Answer &a : ans_) {
    if (a.a == NoticeAnswer::None || strcmp(a.id, id) != 0) continue;
    if (now_ms - a.ms >= NOTICE_ANSWER_KEEP_MS) return NoticeAnswer::None;
    if (rev) *rev = a.rev;
    return a.a;
  }
  return NoticeAnswer::None;
}

PostResult NoticeBoard::post(Notice &n, uint32_t now_ms, uint32_t *rev) {
  if (rev) *rev = 0;
  if (n.id[0]) forget_answer(n.id);
  // Pedido com resposta nunca é "repetido": dois "Rodar git" seguidos podem
  // ser comandos diferentes, e o segundo tem que trocar a revisão.
  if (!n.actions && has_last_ && strcmp(last_given_id_, n.id) == 0 &&
      strcmp(last_title_, n.title) == 0 && now_ms - last_ms_ < NOTICE_DEDUP_MS) {
    memcpy(n.id, last_id_, sizeof(n.id));
    int i = find(n.id);
    if (rev && i >= 0) *rev = e_[i].seq;
    return PostResult::Repeated;
  }
  has_last_ = true;
  memcpy(last_given_id_, n.id, sizeof(last_given_id_));
  memcpy(last_title_, n.title, sizeof(last_title_));
  last_ms_ = now_ms;
  if (!n.id[0]) snprintf(n.id, sizeof(n.id), "n%lu", (unsigned long)++auto_id_);
  memcpy(last_id_, n.id, sizeof(last_id_));

  // Um por id: sai o anterior.
  bool replaced = false;
  for (uint8_t i = 0; i < count_; i++) {
    if (strcmp(e_[i].n.id, n.id) == 0) {
      erase(i);
      replaced = true;
      break;
    }
  }
  if (count_ == NOTICE_BOARD_CAP) {
    // Cheio: sai o menos importante (o último), se o novo for mais importante.
    // Empate: o novo é mais recente e perde (o mais antigo vai antes).
    if ((uint8_t)n.priority >= (uint8_t)e_[count_ - 1].n.priority) {
      if (replaced) changed();
      return PostResult::Dropped;
    }
    count_--;
  }
  e_[count_].n = n;
  e_[count_].seq = ++seq_;
  e_[count_].posted_ms = now_ms;
  if (rev) *rev = seq_;
  count_++;
  sort();
  changed();
  return PostResult::Added;
}

uint8_t NoticeBoard::remove(const char *id, const char *tag) {
  uint8_t removed = 0;
  for (uint8_t i = 0; i < count_;) {
    if (strcmp(e_[i].n.id, id) == 0 && (!tag[0] || strcmp(e_[i].n.tag, tag) == 0)) {
      erase(i);
      removed++;
    } else {
      i++;
    }
  }
  // Respondeu: o mesmo aviso logo depois é novo.
  if (strcmp(last_id_, id) == 0) has_last_ = false;
  if (removed) changed();
  return removed;
}

uint8_t NoticeBoard::remove_all() {
  uint8_t removed = count_;
  count_ = 0;
  has_last_ = false;
  if (removed) changed();
  return removed;
}

uint8_t NoticeBoard::expire(uint32_t now_ms) {
  uint8_t removed = 0;
  for (uint8_t i = 0; i < count_;) {
    uint32_t t = e_[i].n.timeout_s;
    if (t && now_ms - e_[i].posted_ms >= t * 1000u) {
      erase(i);
      removed++;
    } else {
      i++;
    }
  }
  if (removed) changed();
  return removed;
}

void NoticeBoard::dismiss(uint8_t i) {
  if (i >= count_) return;
  erase(i);
  changed();
}
