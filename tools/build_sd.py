#!/usr/bin/env python3
"""Gera o conteúdo do cartão SD do CYDex a partir da PokeAPI.

Saída (copiar o conteúdo de --out pra raiz de um cartão FAT32):

    cydex/species.bin            cabeçalho + 1025 registros de 64 B
    cydex/sprites_96.bin         todos os 96x96 RGB565 chroma key (Ficha, Vitrine 2x)
    cydex/sprites_64.bin         64x64 (Evolução)
    cydex/sprites_48.bin         48x48 (Grade, Time, Hall da Fama)
    cydex/sprites_24.bin         24x24 (Caixas e lista do Hall da Fama)
    cydex/moves.bin              golpes 1..354 (nome, tipo, PP base)
    cydex/items_g3.bin           nomes dos itens pelo índice do Gen3
    saves/                       pasta vazia pros .sav/.srm

Os layouts binários espelham lib/core/src/dex/SpeciesRecord.h,
lib/core/src/dex/SpriteFile.h e lib/core/src/dex/NameTable.h. Se um mudar, o
outro muda junto.

Sprites ficam num arquivo por tamanho, com registros de tamanho fixo. Antes
eram 1025 arquivos por pasta, mas no FAT32 cada abertura varre a pasta toda
(~560 ms por sprite na placa).

As respostas da PokeAPI ficam em tools/.cache (a PokeAPI pede cache local).
Rodar de novo só baixa o que falta.

Uso:
    python3 tools/build_sd.py --out build/sd
    python3 tools/build_sd.py --out build/sd --limit 12    # teste rápido
    python3 tools/build_sd.py --out build/sd --names-lang fr   # nomes em francês

Nomes das espécies: a PokeAPI não tem pt-BR, então o padrão é "en". Os nomes
oficiais em português são os mesmos do inglês. Idiomas com alfabeto fora do
Latin-1 (ja, ko, zh) não cabem nas fontes do firmware e o script recusa.
"""

from __future__ import annotations

import argparse
import http.client
import io
import json
import struct
import threading
import sys
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image

API = "https://pokeapi.co/api/v2"
CACHE = Path(__file__).resolve().parent / ".cache"
USER_AGENT = "cydex-build-sd/1.0"

DEX_COUNT = 1025
GEN_START = [1, 152, 252, 387, 494, 650, 722, 810, 906, 1026]

# ---- Formato (espelho de SpeciesRecord.h) ----
SPECIES_FILE_VERSION = 1
HEADER_SIZE = 16
RECORD_SIZE = 64
NAME_SIZE = 16
FLAG_HAS_SPRITE = 0x01
TYPE_NONE = 0xFF

# Ordem = SpeciesType em lib/core/src/dex/Types.h
TYPES = [
    "normal", "fire", "water", "grass", "electric", "ice", "fighting", "poison", "ground",
    "flying", "psychic", "bug", "rock", "ghost", "dragon", "dark", "steel", "fairy",
]
STATS = ["hp", "attack", "defense", "special-attack", "special-defense", "speed"]
ROMAN = {"i": 1, "ii": 2, "iii": 3, "iv": 4, "v": 5, "vi": 6, "vii": 7, "viii": 8, "ix": 9}

# ---- Sprites (LVGL 8: lv_img_header_t + RGB565 LE, LV_COLOR_16_SWAP=0) ----
LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED = 6
CHROMA_KEY_565 = 0x07E0  # lv_color_hex(0x00FF00) == LV_COLOR_CHROMA_KEY no lv_conf.h
CHROMA_SAFE_565 = 0x07C0  # pixel opaco que colidiria com a chave
SPRITE_SIZES = (96, 64, 48, 24)

# ---- Tabelas de nomes (espelho de NameTable.h) ----
NAME_TABLE_VERSION = 1
NAME_TABLE_HEADER_SIZE = 16
TABLE_NAME_SIZE = 24
MOVE_RECORD_SIZE = 28
ITEM_RECORD_SIZE = 24
GEN3_MOVE_COUNT = 354  # Pound..Psycho Boost (ids iguais aos da PokeAPI)
GROWTH_RATES = {"slow": 1, "medium": 2, "fast": 3, "medium-slow": 4, "slow-then-very-fast": 5,
                "fast-then-very-slow": 6}
SPRITE_FILE_VERSION = 1
SPRITE_HEADER_SIZE = 16


def encode_sprite_header(count: int, size: int) -> bytes:
    return struct.pack("<4sHHHI2x", b"CYSP", SPRITE_FILE_VERSION, count, size, 4 + size * size * 2)


def empty_sprite(size: int) -> bytes:
    header = struct.pack("<I", LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED | (size << 10) | (size << 21))
    return header + struct.pack("<H", CHROMA_KEY_565) * (size * size)

# Nomes que a fonte (Latin-1) não tem ou que não cabem.
NAME_FIXES = {
    "♀": " F",  # ♀
    "♂": " M",  # ♂
    "’": "'",   # ’
}

# Números de referência que o script confere no fim.
KNOWN = {1: "Bulbasaur", 25: "Pikachu", 150: "Mewtwo", 386: "Deoxys", 669: "Flabébé",
         1025: "Pecharunt"}


# ---------------------------------------------------------------- rede/cache

def fetch(url: str, binary: bool = False):
    """GET com cache em disco e retry."""
    key = url.replace("https://", "").replace("/", "_").strip("_")
    path = CACHE / (key + (".bin" if binary else ".json"))
    if path.exists():
        data = path.read_bytes()
        return data if binary else json.loads(data)

    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(5):
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                data = resp.read()
            break
        except (urllib.error.URLError, TimeoutError, ConnectionError, http.client.HTTPException) as exc:
            if isinstance(exc, urllib.error.HTTPError) and exc.code == 404:
                raise
            time.sleep(1.5 * (attempt + 1))
    else:
        raise RuntimeError(f"falhou depois de 5 tentativas: {url}")

    CACHE.mkdir(parents=True, exist_ok=True)
    # .tmp único por thread: duas threads podem baixar a mesma cadeia evolutiva.
    tmp = path.with_suffix(f".{threading.get_ident()}.tmp")
    tmp.write_bytes(data)
    tmp.replace(path)
    return data if binary else json.loads(data)


# ---------------------------------------------------------------- dados

NAMES_LANG = "en"


def localized_name(entry: dict, size: int = NAME_SIZE) -> str:
    """Nome no idioma de --names-lang, com o inglês como reserva."""
    by_lang = {e["language"]["name"]: e["name"] for e in entry["names"]}
    name = by_lang.get(NAMES_LANG) or by_lang.get("en") or entry["name"].replace("-", " ").title()
    for bad, good in NAME_FIXES.items():
        name = name.replace(bad, good)
    if any(ord(c) > 0xFF for c in name):
        raise ValueError(f"nome fora do Latin-1: {name!r}")
    if len(name.encode("utf-8")) > size - 1:
        raise ValueError(f"nome longo demais pra {size - 1} bytes: {name!r}")
    return name


def species_name(species: dict) -> str:
    return localized_name(species)


def id_from_url(url: str) -> int:
    return int(url.rstrip("/").split("/")[-1])


def main_evolution_line(chain: dict, number: int) -> list[tuple[int, int]]:
    """Linha principal com até 3 estágios: [(dex, nível pra chegar nele)].

    Se a espécie estiver num ramo (Eevee -> Vaporeon), usa o caminho até ela.
    A partir dela, segue sempre o primeiro filho.
    """

    def find(node, path):
        here = path + [node]
        if id_from_url(node["species"]["url"]) == number:
            return here
        for child in node["evolves_to"]:
            found = find(child, here)
            if found:
                return found
        return None

    path = find(chain["chain"], []) or [chain["chain"]]
    while path[-1]["evolves_to"] and len(path) < 3:
        path.append(path[-1]["evolves_to"][0])

    line = []
    for node in path[-3:] if len(path) > 3 else path:
        details = node.get("evolution_details") or []
        level = (details[0].get("min_level") or 0) if details else 0
        line.append((id_from_url(node["species"]["url"]), min(level, 255)))
    return line


def load_species(n: int) -> dict:
    species = fetch(f"{API}/pokemon-species/{n}")
    default = next(v for v in species["varieties"] if v["is_default"])
    pokemon = fetch(default["pokemon"]["url"].rstrip("/"))
    chain = fetch(species["evolution_chain"]["url"].rstrip("/"))

    types = sorted(pokemon["types"], key=lambda t: t["slot"])
    stats = {s["stat"]["name"]: s["base_stat"] for s in pokemon["stats"]}
    gen = ROMAN[species["generation"]["name"].split("-")[1]]
    sprite_url = pokemon["sprites"]["front_default"]

    return {
        "number": n,
        "name": species_name(species),
        "type1": TYPES.index(types[0]["type"]["name"]),
        "type2": TYPES.index(types[1]["type"]["name"]) if len(types) > 1 else TYPE_NONE,
        "height_dm": pokemon["height"],
        "weight_hg": pokemon["weight"],
        "generation": gen,
        "stats": [min(stats[s], 255) for s in STATS],
        "growth": GROWTH_RATES.get(species["growth_rate"]["name"], 0),
        "evo": main_evolution_line(chain, n),
        "sprite": fetch(sprite_url, binary=True) if sprite_url else None,
    }


# ---------------------------------------------------------------- binários

def encode_header(count: int) -> bytes:
    return struct.pack("<4sHHH6x", b"CYDX", SPECIES_FILE_VERSION, count, RECORD_SIZE)


def encode_record(sp: dict) -> bytes:
    evo = sp["evo"]
    numbers = [e[0] for e in evo] + [0] * (3 - len(evo))
    levels = [e[1] for e in evo[1:]] + [0] * (2 - len(evo[1:]))
    rec = struct.pack(
        "<16sBBHHB6sB3H2sB",
        sp["name"].encode("utf-8"),
        sp["type1"],
        sp["type2"],
        sp["height_dm"],
        sp["weight_hg"],
        sp["generation"],
        bytes(sp["stats"]),
        len(evo),
        *numbers,
        bytes(levels),
        FLAG_HAS_SPRITE if sp["sprite"] else 0,
    ) + bytes([sp["growth"]])
    assert len(rec) == 40, len(rec)
    return rec + bytes(RECORD_SIZE - len(rec))


def to_96(png: bytes) -> Image.Image:
    img = Image.open(io.BytesIO(png)).convert("RGBA")
    if img.size != (96, 96):
        # Alguns sprites novos vêm maiores: reduz mantendo proporção e centraliza.
        img.thumbnail((96, 96), Image.NEAREST)
        canvas = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
        canvas.paste(img, ((96 - img.width) // 2, (96 - img.height) // 2))
        img = canvas
    return img


def encode_sprite(img: Image.Image, size: int) -> bytes:
    if img.width != size:
        # BOX faz média por área (o Pillow pré-multiplica o alpha). Depois o
        # alpha é cortado em 50%, então a borda continua dura, como pixel art.
        img = img.resize((size, size), Image.BOX)
    header = struct.pack("<I", LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED | (size << 10) | (size << 21))
    out = bytearray(header)
    raw = img.tobytes()
    for i in range(0, len(raw), 4):
        r, g, b, a = raw[i], raw[i + 1], raw[i + 2], raw[i + 3]
        if a < 128:
            px = CHROMA_KEY_565
        else:
            px = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            if px == CHROMA_KEY_565:
                px = CHROMA_SAFE_565
        out += struct.pack("<H", px)
    return bytes(out)


def encode_name_table_header(count: int, record_size: int) -> bytes:
    return struct.pack("<4sHHH6x", b"CYNT", NAME_TABLE_VERSION, count, record_size)


def load_move(n: int) -> bytes:
    move = fetch(f"{API}/move/{n}")
    name = localized_name(move, TABLE_NAME_SIZE)
    # Golpes de tipo "shadow"/"unknown" não existem no Gen3; caem em Normal.
    t = move["type"]["name"]
    type_id = TYPES.index(t) if t in TYPES else 0
    rec = struct.pack("<24sBB2x", name.encode("utf-8"), type_id, move["pp"] or 0)
    assert len(rec) == MOVE_RECORD_SIZE
    return rec


def gen3_item_names(workers: int) -> dict[int, str]:
    """{índice do item no Gen3: nome}, pelos game_indices da PokeAPI."""
    listing = fetch(f"{API}/item?limit=3000")
    urls = [e["url"].rstrip("/") for e in listing["results"]]

    def one(url):
        item = fetch(url)
        idx = [g["game_index"] for g in item["game_indices"] if g["generation"]["name"] == "generation-iii"]
        if not idx:
            return None
        try:
            return idx[0], localized_name(item, TABLE_NAME_SIZE)
        except ValueError:
            return idx[0], item["name"].replace("-", " ").title()[:TABLE_NAME_SIZE - 1]

    out: dict[int, str] = {}
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for got in pool.map(one, urls):
            if got:
                out.setdefault(got[0], got[1])
    return out


# ---------------------------------------------------------------- main

def validate(records: list[dict], count: int) -> None:
    assert len(records) == count
    for i, sp in enumerate(records, start=1):
        assert sp["number"] == i
        g = sp["generation"]
        assert GEN_START[g - 1] <= i < GEN_START[g], f"#{i} geração {g} fora da faixa"
    for n, name in (KNOWN.items() if NAMES_LANG == "en" else ()):
        if n <= count:
            got = records[n - 1]["name"]
            assert got == name, f"#{n}: esperado {name!r}, veio {got!r}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, required=True, help="pasta de saída (raiz do cartão)")
    ap.add_argument("--limit", type=int, default=DEX_COUNT, help="só as N primeiras espécies")
    ap.add_argument("--workers", type=int, default=8)
    ap.add_argument("--names-lang", default="en",
                    help="idioma dos nomes das espécies na PokeAPI (en, es, fr, de, it...)")
    args = ap.parse_args()
    global NAMES_LANG
    NAMES_LANG = args.names_lang
    count = max(1, min(args.limit, DEX_COUNT))

    print(f"baixando {count} espécies (cache em {CACHE})...")
    records: list[dict] = []
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        for i, sp in enumerate(pool.map(load_species, range(1, count + 1)), start=1):
            records.append(sp)
            if i % 50 == 0 or i == count:
                print(f"  {i}/{count}")

    validate(records, count)

    root = args.out / "cydex"
    root.mkdir(parents=True, exist_ok=True)
    (args.out / "saves").mkdir(parents=True, exist_ok=True)

    with open(root / "species.bin", "wb") as f:
        f.write(encode_header(count))
        for sp in records:
            f.write(encode_record(sp))

    missing = [sp["number"] for sp in records if not sp["sprite"]]
    images = {sp["number"]: to_96(sp["sprite"]) for sp in records if sp["sprite"]}
    for size in SPRITE_SIZES:
        path = root / f"sprites_{size}.bin"
        with open(path, "wb") as f:
            f.write(encode_sprite_header(count, size))
            for sp in records:
                img = images.get(sp["number"])
                data = encode_sprite(img, size) if img else empty_sprite(size)
                assert len(data) == 4 + size * size * 2
                f.write(data)
        expected = SPRITE_HEADER_SIZE + count * (4 + size * size * 2)
        assert path.stat().st_size == expected, (path, path.stat().st_size, expected)

    print(f"baixando {GEN3_MOVE_COUNT} golpes...")
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        moves = list(pool.map(load_move, range(1, GEN3_MOVE_COUNT + 1)))
    with open(root / "moves.bin", "wb") as f:
        f.write(encode_name_table_header(GEN3_MOVE_COUNT, MOVE_RECORD_SIZE))
        for rec in moves:
            f.write(rec)
    assert moves[0][:5] == b"Pound" or NAMES_LANG != "en", moves[0]

    print("baixando os itens (índice do Gen3)...")
    items = gen3_item_names(args.workers)
    item_count = max(items) if items else 0
    with open(root / "items_g3.bin", "wb") as f:
        f.write(encode_name_table_header(item_count, ITEM_RECORD_SIZE))
        for i in range(1, item_count + 1):
            f.write(struct.pack("<24s", items.get(i, "").encode("utf-8")))
    if NAMES_LANG == "en":
        assert items.get(200) == "Leftovers", items.get(200)
    print(f"ok: moves.bin ({GEN3_MOVE_COUNT}), items_g3.bin ({len(items)} de 1..{item_count})")

    size = (root / "species.bin").stat().st_size
    assert size == HEADER_SIZE + count * RECORD_SIZE, size
    print(f"ok: {root / 'species.bin'} ({size} B), {count - len(missing)} sprites em "
          + ", ".join(f"sprites_{px}.bin" for px in SPRITE_SIZES))
    if missing:
        print(f"sem sprite: {missing}")
    longest = max(records, key=lambda s: len(s["name"].encode()))
    print(f"nome mais longo: {longest['name']!r} ({len(longest['name'].encode())} B)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
