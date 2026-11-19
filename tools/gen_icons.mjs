// Gera os ícones do LVGL em src/ui/icons/ a partir dos SVGs do canvas.
// Cada ícone vira uma imagem A8 (só alpha) que a UI pinta com
// lv_obj_set_style_img_recolor, então a cor fica no tema, não no arquivo.
//
// Uso: (cd tools && npm install && npm run icons)
import { Resvg } from "@resvg/resvg-js";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const out = join(here, "..", "src", "ui", "icons");

// Traços copiados dos SVGs do canvas (viewBox 24x24, stroke em branco).
const SD = '<path d="M7 2h8l4 4v15a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1z"/><path d="M10 2v3M13 2v3M16 3v2"/>';
const PATHS = {
  sd: SD,
  sd_error: SD + '<path d="M10 12l4 4M14 12l-4 4"/>',
  refresh: '<path d="M20 12a8 8 0 1 1-2.3-5.7"/><path d="M20 4v5h-5"/>',
  monitor: '<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4"/>',
  list: '<path d="M9 6h11M9 12h11M9 18h11"/><circle cx="4.5" cy="6" r="1.3" fill="#fff"/><circle cx="4.5" cy="12" r="1.3" fill="#fff"/><circle cx="4.5" cy="18" r="1.3" fill="#fff"/>',
  search: '<circle cx="11" cy="11" r="7"/><path d="M20 20l-3.6-3.6"/>',
  tag: '<path d="M3 12V4a1 1 0 0 1 1-1h8l9 9-9 9z"/><circle cx="8" cy="8" r="1.5"/>',
  bars: '<path d="M5 20V11M12 20V4M19 20v-6"/>',
  sliders: '<path d="M4 7h9M17 7h3M4 17h3M11 17h9"/><circle cx="15" cy="7" r="2"/><circle cx="9" cy="17" r="2"/>',
  back: '<path d="M15 18l-6-6 6-6"/>',
  forward: '<path d="M9 18l6-6-6-6"/>',
  check: '<path d="M5 12.5l4.5 4.5L19 7.5"/>',
  grid: '<rect x="4" y="4" width="6.5" height="6.5" rx="1"/><rect x="13.5" y="4" width="6.5" height="6.5" rx="1"/><rect x="4" y="13.5" width="6.5" height="6.5" rx="1"/><rect x="13.5" y="13.5" width="6.5" height="6.5" rx="1"/>',
  backspace: '<path d="M9 5h11a1 1 0 0 1 1 1v12a1 1 0 0 1-1 1H9l-6-7z"/><path d="M12 9.5l5 5M17 9.5l-5 5"/>',
  arrow: '<path d="M5 12h14M13 6l6 6-6 6"/>',
  close: '<path d="M6 6l12 12M18 6L6 18"/>',
  globe: '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18"/>',
};

// [nome do ícone no código, traço, tamanho px, espessura do traço]
const ICONS = [
  ["sd_16", "sd", 16, 2.2],              // Ajustes › Sistema
  ["sd_error_44", "sd_error", 44, 1.8],
  ["refresh_18", "refresh", 18, 2.4],
  ["monitor_14", "monitor", 14, 2.2],
  ["monitor_16", "monitor", 16, 2.2],
  ["globe_16", "globe", 16, 2.2],
  ["list_26", "list", 26, 2.2],
  ["list_18", "list", 18, 2.2],
  ["grid_18", "grid", 18, 2.2],
  ["search_20", "search", 20, 2.2],
  ["search_18", "search", 18, 2.2],
  ["search_14", "search", 14, 2.4],      // campo da Busca
  ["search_12", "search", 12, 2.6],      // chip de filtro (06c)
  ["close_12", "close", 12, 3],          // limpar o campo da Busca
  ["close_10", "close", 10, 3.4],        // ✕ do chip de filtro
  ["tag_20", "tag", 20, 2.2],
  ["tag_18", "tag", 18, 2.2],            // cabeçalho da Lista/Grade -> Tipos
  ["bars_20", "bars", 20, 2.2],
  ["sliders_20", "sliders", 20, 2.2],
  ["back_20", "back", 20, 2.2],          // cabeçalho
  ["prev_20", "back", 20, 2.6],          // rodapé de paginação
  ["next_20", "forward", 20, 2.6],
  ["forward_18", "forward", 18, 2.4],    // linhas "›" dos Ajustes; › da Ficha
  ["prev_18", "back", 18, 2.4],          // ‹ da Ficha
  ["arrow_18", "arrow", 18, 2.4],        // entre estágios da Evolução
  ["backspace_18", "backspace", 18, 2],
];

// Ícones com viewBox próprio (marcas de status, 14x14 no canvas).
// [nome, viewBox (largura), conteúdo SVG com stroke/fill já definidos, largura px, altura do viewBox se não for quadrado]
const RAW = [
  // Check do "capturado" (vai sobre um círculo accent desenhado pelo LVGL).
  ["check_14", 14, '<path d="M4 7.2l2 2 4-4.4" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>', 14],
  ["check_12", 14, '<path d="M4 7.2l2 2 4-4.4" fill="none" stroke="#fff" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/>', 12],
  // "Desconhecido": anel tracejado (o LVGL 8 não faz borda tracejada).
  ["ring_dashed_12", 14, '<circle cx="7" cy="7" r="6" fill="none" stroke="#fff" stroke-width="2.2" stroke-dasharray="2 2.4"/>', 12],
  ["ring_dashed_14", 14, '<circle cx="7" cy="7" r="6" fill="none" stroke="#fff" stroke-width="2" stroke-dasharray="2 2.4"/>', 14],
  // Caixa tracejada do sprite ausente na Grade (48x48, borda 1 px, raio 4).
  // Sprite ausente na Ficha (96, raio 6) e na Evolução (64, raio 4).
  ["dash_box_96", 96, '<rect x="0.5" y="0.5" width="95" height="95" rx="6" fill="none" stroke="#fff" stroke-width="1" stroke-dasharray="3 2"/>', 96],
  ["dash_box_64", 64, '<rect x="0.5" y="0.5" width="63" height="63" rx="4" fill="none" stroke="#fff" stroke-width="1" stroke-dasharray="3 2"/>', 64],
  // Selo "Não visto" da Ficha (112x24, raio 12).
  ["dash_pill_112", 112, '<rect x="0.5" y="0.5" width="111" height="23" rx="11.5" fill="none" stroke="#fff" stroke-width="1" stroke-dasharray="3 2"/>', 112, 24],
  // Sombra no chão da Vitrine: elipse 140x22 com borda que some (gradiente
  // radial). O LVGL não desenha elipse; pintada com a cor do tipo a 18%.
  ["floor_140", 140, '<defs><radialGradient id="g" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#fff" stop-opacity="1"/><stop offset="0.55" stop-color="#fff" stop-opacity="0.85"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient></defs><ellipse cx="70" cy="11" rx="70" ry="11" fill="url(#g)"/>', 140, 22],
  // ▶ cheio do "Iniciar vitrine".
  ["play_18", 24, '<path d="M8 5v14l11-7z" fill="#fff"/>', 18],  // ▶ do cabeçalho do 05b
  ["dash_box_48", 48, '<rect x="0.5" y="0.5" width="47" height="47" rx="4" fill="none" stroke="#fff" stroke-width="1" stroke-dasharray="3 2"/>', 48],
];

function rasterize(paths, size, stroke, viewBox = 24, raw = false, viewBoxH = viewBox) {
  const attrs = raw ? "" : ` fill="none" stroke="#fff" stroke-width="${stroke}" stroke-linecap="round" stroke-linejoin="round"`;
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${viewBox} ${viewBoxH}"${attrs}>${paths}</svg>`;
  const img = new Resvg(svg, { fitTo: { mode: "width", value: size } }).render();
  const alpha = new Uint8Array(img.width * img.height);
  for (let i = 0; i < alpha.length; i++) alpha[i] = img.pixels[i * 4 + 3];
  return { w: img.width, h: img.height, alpha };
}

mkdirSync(out, { recursive: true });
let c = `// Gerado por tools/gen_icons.mjs. Não editar à mão.\n#include "icons.h"\n\n`;
let h = `#pragma once\n\n// Gerado por tools/gen_icons.mjs. Não editar à mão.\n// Imagens A8: pintar com lv_obj_set_style_img_recolor(+_opa COVER).\n\n#include <lvgl.h>\n\n`;
const jobs = [
  ...ICONS.map(([name, key, size, stroke]) => [name, () => rasterize(PATHS[key], size, stroke)]),
  ...RAW.map(([name, vb, svg, size, vbH]) => [name, () => rasterize(svg, size, 0, vb, true, vbH ?? vb)]),
];
for (const [name, make] of jobs) {
  const { w, h: hh, alpha } = make();
  const rows = [];
  for (let y = 0; y < hh; y++) rows.push("  " + Array.from(alpha.slice(y * w, (y + 1) * w), (v) => `0x${v.toString(16).padStart(2, "0")}`).join(","));
  c += `static const uint8_t ${name}_map[] = {\n${rows.join(",\n")}\n};\n`;
  c += `const lv_img_dsc_t icon_${name} = {\n  .header = {.cf = LV_IMG_CF_ALPHA_8BIT, .always_zero = 0, .reserved = 0, .w = ${w}, .h = ${hh}},\n  .data_size = ${w * hh},\n  .data = ${name}_map,\n};\n\n`;
  h += `extern const lv_img_dsc_t icon_${name};\n`;
  console.log(`ok icon_${name} ${w}x${hh}`);
}
writeFileSync(join(out, "icons.c"), c);
writeFileSync(join(out, "icons.h"), `${h.replace("#include <lvgl.h>\n\n", "#include <lvgl.h>\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")}\n#ifdef __cplusplus\n}\n#endif\n`);
