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
  wifi: '<path d="M2 8.5a14.5 14.5 0 0 1 20 0"/><path d="M5 12a10 10 0 0 1 14 0"/><path d="M8.5 15.5a5 5 0 0 1 7 0"/><circle cx="12" cy="19" r="1"/>',
  wifi_off: '<path d="M2 8.5a14.5 14.5 0 0 1 20 0"/><path d="M5 12a10 10 0 0 1 14 0"/><path d="M8.5 15.5a5 5 0 0 1 7 0"/><circle cx="12" cy="19" r="1"/><path d="M3 3l18 18"/>',
  info: '<circle cx="12" cy="12" r="9"/><path d="M12 11v5"/><path d="M12 8h.01"/>',
  lock: '<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>',
  eye: '<path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>',
  eye_off: '<path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/><path d="M3 3l18 18"/>',
  shift: '<path d="M12 4l8 8h-4.5v7h-7v-7H4z"/>',
  shield: '<path d="M12 3l8 3v6c0 5-3.5 8-8 9-4.5-1-8-4-8-9V6z"/><path d="M12 9v4"/><path d="M12 16h.01"/>',
  clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
  sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
  compass: '<circle cx="12" cy="12" r="9"/><path d="M15.5 8.5l-2 5-5 2 2-5z"/>',
  chip: '<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/>',
  check_circle: '<circle cx="12" cy="12" r="9"/><path d="M8 12.5l2.5 2.5L16 9"/>',
  alert: '<path d="M12 3.5L21.5 20h-19z"/><path d="M12 10v4"/><path d="M12 17h.01"/>',
  bell: '<path d="M6 16V11a6 6 0 0 1 12 0v5l1.5 2h-15z"/><path d="M10 21h4"/>',
  cartridge: '<path d="M6 3h9l3 3v15H6z"/><path d="M9 3v4h6V3"/><rect x="9" y="11" width="6" height="6" rx="1"/>',
  trophy: '<path d="M8 4h8v5a4 4 0 0 1-8 0z"/><path d="M8 6H5a3 3 0 0 0 3 4M16 6h3a3 3 0 0 1-3 4M12 13v4M8 20h8M10 17h4"/>',
  up: '<path d="M12 19V5M6 11l6-6 6 6"/>',
  down: '<path d="M12 5v14M6 13l6 6 6-6"/>',
  male: '<circle cx="10" cy="14" r="6"/><path d="M14.5 9.5L20 4M15 4h5v5"/>',
  female: '<circle cx="12" cy="9" r="6"/><path d="M12 15v7M9 19h6"/>',
  terminal: '<path d="M5 7l5 5-5 5"/><path d="M13 18h6"/>',
};

// [nome do ícone no código, traço, tamanho px, espessura do traço]
const ICONS = [
  ["sd_16", "sd", 16, 2.2],              // Ajustes › Sistema
  ["sd_error_44", "sd_error", 44, 1.8],
  ["refresh_18", "refresh", 18, 2.4],
  ["monitor_14", "monitor", 14, 2.2],
  ["monitor_16", "monitor", 16, 2.2],
  ["globe_16", "globe", 16, 2.2],        // Sistema › Idioma
  ["sun_16", "sun", 16, 2.2],            // Ajustes › Tela e som
  ["compass_16", "compass", 16, 2.2],    // Ajustes › Navegação
  ["chip_16", "chip", 16, 2.2],          // Ajustes › Sistema
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
  // Wi-Fi (05c, 05d, 05e) e avisos do Claude Code (15, 15b).
  ["wifi_16", "wifi", 16, 2.2],          // linha Wi-Fi do Sistema
  ["wifi_14", "wifi", 14, 2.4],          // faixa da Senha
  ["wifi_off_28", "wifi_off", 28, 2],    // Wi-Fi desligado
  ["info_18", "info", 18, 2.2],          // ⓘ do cabeçalho do Wi-Fi
  ["lock_12", "lock", 12, 2.6],          // rede com senha
  ["lock_14", "lock", 14, 2.4],          // campo da senha
  ["eye_16", "eye", 16, 2.2],
  ["eye_off_16", "eye_off", 16, 2.2],
  ["shift_18", "shift", 18, 2.2],        // ⇧ do teclado (a fonte não tem o glifo)
  // Ícones dos avisos (campo "icon" do POST /notices): 20 na faixa, 16 no balão.
  ["shield_20", "shield", 20, 2.2],
  ["clock_20", "clock", 20, 2.2],
  ["check_circle_20", "check_circle", 20, 2.2],
  ["info_20", "info", 20, 2.2],
  ["alert_20", "alert", 20, 2.2],
  ["bell_20", "bell", 20, 2.2],
  ["shield_16", "shield", 16, 2.4],
  ["clock_16", "clock", 16, 2.4],
  ["check_circle_16", "check_circle", 16, 2.4],
  ["info_16", "info", 16, 2.4],
  ["alert_16", "alert", 16, 2.4],
  ["bell_16", "bell", 16, 2.4],
  ["terminal_20", "terminal", 20, 2.2],
  ["terminal_16", "terminal", 16, 2.4],
  // Indicadores do rodapé da Vitrine (14b, PUT /status): 12 px.
  ["terminal_12", "terminal", 12, 2.6],
  ["info_12", "info", 12, 2.6],
  ["check_circle_12", "check_circle", 12, 2.6],
  ["alert_12", "alert", 12, 2.6],
  ["shield_12", "shield", 12, 2.6],
  ["clock_12", "clock", 12, 2.6],
  ["bell_12", "bell", 12, 2.6],
  ["forward_14", "forward", 14, 2.6],    // › do balão que pede resposta (15c)
  // Saves e Hall da Fama (16–19a)
  ["cartridge_20", "cartridge", 20, 2.2],  // tile do Menu
  ["cartridge_28", "cartridge", 28, 2],    // 16a sem saves
  ["trophy_20", "trophy", 20, 2.2],        // tile do Menu
  ["trophy_14", "trophy", 14, 2.4],        // cards do Hall da Fama
  ["forward_12", "forward", 12, 2.8],      // "Abrir ›", link da espécie no Membro
  ["up_12", "up", 12, 2.8],                // natureza: status que sobe
  ["down_12", "down", 12, 2.8],            // natureza: status que desce
  ["male_12", "male", 12, 2.6],
  ["female_12", "female", 12, 2.6],
  ["male_13", "male", 13, 2.6],
  ["female_13", "female", 13, 2.6],
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
  // ▶ cheio do "Iniciar vitrine".
  ["play_18", 24, '<path d="M8 5v14l11-7z" fill="#fff"/>', 18],  // ▶ do cabeçalho do 05b
  // Ponta do balão da Vitrine (15b): contorno na cor do aviso + miolo surface por cima.
  ["tail_line_8", 8, '<path d="M8 0.5L0.8 7L8 13.5" fill="none" stroke="#fff" stroke-width="1"/>', 8, 14],
  ["tail_fill_8", 8, '<path d="M8.5 1.6L2.2 7L8.5 12.4z" fill="#fff"/>', 8, 14],
  // Permitir desarmado da folha 15c (67x40, raio 8): o LVGL 8 não faz borda tracejada.
  ["dash_btn_67", 67, '<rect x="0.5" y="0.5" width="66" height="39" rx="7.5" fill="none" stroke="#fff" stroke-width="1" stroke-dasharray="3 2"/>', 67, 40],
  // ★ de brilhante (a fonte não tem o glifo).
  ["star_13", 24, '<path d="M12 2l2.9 6.6 7.1.6-5.4 4.7 1.6 7-6.2-3.7-6.2 3.7 1.6-7L2 9.2l7.1-.6z" fill="#fff"/>', 13],
  ["star_12", 24, '<path d="M12 2l2.9 6.6 7.1.6-5.4 4.7 1.6 7-6.2-3.7-6.2 3.7 1.6-7L2 9.2l7.1-.6z" fill="#fff"/>', 12],
  ["star_9", 24, '<path d="M12 2l2.9 6.6 7.1.6-5.4 4.7 1.6 7-6.2-3.7-6.2 3.7 1.6-7L2 9.2l7.1-.6z" fill="#fff"/>', 9],
  // Insígnia: losango (quadrado girado 45°), cheio ou só o contorno.
  ["badge_10", 14, '<rect x="3" y="3" width="8" height="8" rx="1.5" transform="rotate(45 7 7)" fill="#fff"/>', 10],
  ["badge_line_10", 14, '<rect x="3.5" y="3.5" width="7" height="7" rx="1.3" transform="rotate(45 7 7)" fill="none" stroke="#fff" stroke-width="1.6"/>', 10],
  ["badge_7", 14, '<rect x="3" y="3" width="8" height="8" rx="1.5" transform="rotate(45 7 7)" fill="#fff"/>', 7],
  ["badge_line_7", 14, '<rect x="3.5" y="3.5" width="7" height="7" rx="1.3" transform="rotate(45 7 7)" fill="none" stroke="#fff" stroke-width="1.8"/>', 7],
  // Ícone 24 ausente (Caixas, Hall da Fama) e vaga vazia do Time.
  ["dash_box_24", 24, '<rect x="0.5" y="0.5" width="23" height="23" rx="3" fill="none" stroke="#fff" stroke-width="1" stroke-dasharray="3 2"/>', 24],
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
