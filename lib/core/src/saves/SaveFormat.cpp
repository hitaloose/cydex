#include "SaveFormat.h"

#include "saves/gen3/Gen3Dex.h"

static const SaveFormat *const kFormats[] = {
    &GEN3_SAVE_FORMAT,
};

const char *save_merge_dex(const SaveReader &r, DexStatus &out) {
  for (const SaveFormat *f : kFormats) {
    if (!f->detect(r)) continue;
    if (f->merge_dex(r, out)) return f->name;
  }
  return nullptr;
}

bool save_open(const SaveReader &r, SaveHandle *out) {
  for (const SaveFormat *f : kFormats) {
    if (!f->detect(r)) continue;
    out->format = f;
    out->reader = r;
    if (f->open(*out)) return true;
  }
  out->format = nullptr;
  return false;
}
