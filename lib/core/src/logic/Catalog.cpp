#include "Catalog.h"

uint8_t CatalogQuery::type_count() const {
  return (type_a != SpeciesType::None) + (type_b != SpeciesType::None);
}

bool CatalogQuery::has_type(SpeciesType t) const {
  return t != SpeciesType::None && (type_a == t || type_b == t);
}

bool CatalogQuery::toggle_type(SpeciesType t) {
  if (t == SpeciesType::None) return false;
  if (type_a == t) {
    type_a = type_b;
    type_b = SpeciesType::None;
    return true;
  }
  if (type_b == t) {
    type_b = SpeciesType::None;
    return true;
  }
  if (type_a == SpeciesType::None) {
    type_a = t;
    return true;
  }
  if (type_b == SpeciesType::None) {
    type_b = t;
    return true;
  }
  return false;
}

void CatalogQuery::pick_type(SpeciesType t) {
  if (toggle_type(t)) return;
  type_a = type_b;  // cheio: sai o mais antigo
  type_b = t;
}

void CatalogQuery::clear_refinements() {
  text[0] = '\0';
  type_a = type_b = SpeciesType::None;
}

namespace {

bool matches_with_key(const CatalogQuery &q, const SearchKey *key, uint16_t n, const DexStatus &st,
                      TypeLookup types, NameLookup names) {
  if (key && !search_matches(*key, n, names ? names(n) : nullptr, st.is_seen(n))) return false;
  switch (q.status) {
    case StatusFilter::Caught:
      if (!st.is_caught(n)) return false;
      break;
    case StatusFilter::Missing:
      if (st.is_caught(n)) return false;
      break;
    case StatusFilter::All:
      break;
  }
  if (q.type_count() == 0) return true;
  if (!types) return false;
  SpeciesType t1 = SpeciesType::None, t2 = SpeciesType::None;
  types(n, &t1, &t2);
  auto has = [&](SpeciesType t) { return t == SpeciesType::None || t1 == t || t2 == t; };
  return has(q.type_a) && has(q.type_b);
}

}  // namespace

bool catalog_matches(const CatalogQuery &q, uint16_t n, const DexStatus &st, TypeLookup types,
                     NameLookup names) {
  SearchKey key;
  bool has_key = search_key_make(q.text, &key);
  return matches_with_key(q, has_key ? &key : nullptr, n, st, types, names);
}

uint16_t catalog_build(const CatalogQuery &q, uint16_t count, const DexStatus &st,
                       TypeLookup types, uint16_t *out, NameLookup names) {
  SearchKey key;  // dobrada uma vez só
  bool has_key = search_key_make(q.text, &key);
  uint16_t len = 0;
  for (uint16_t n = 1; n <= count; n++) {
    if (matches_with_key(q, has_key ? &key : nullptr, n, st, types, names)) out[len++] = n;
  }
  return len;
}

uint16_t catalog_index_at_or_after(const uint16_t *list, uint16_t len, uint16_t n) {
  if (len == 0) return 0;
  // Lista ordenada: busca binária.
  uint16_t lo = 0, hi = len;
  while (lo < hi) {
    uint16_t mid = (uint16_t)((lo + hi) / 2);
    if (list[mid] < n) lo = (uint16_t)(mid + 1);
    else hi = mid;
  }
  return lo < len ? lo : (uint16_t)(len - 1);
}

void catalog_neighbors(const uint16_t *list, uint16_t len, uint16_t n, uint16_t *prev,
                       uint16_t *next) {
  *prev = 0;
  *next = 0;
  if (len == 0) return;
  // Primeiro índice com list[i] >= n.
  uint16_t lo = 0, hi = len;
  while (lo < hi) {
    uint16_t mid = (uint16_t)((lo + hi) / 2);
    if (list[mid] < n) lo = (uint16_t)(mid + 1);
    else hi = mid;
  }
  if (lo > 0) *prev = list[lo - 1];
  uint16_t after = (lo < len && list[lo] == n) ? (uint16_t)(lo + 1) : lo;
  if (after < len) *next = list[after];
}
