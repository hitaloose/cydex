// Montagem de DOM sem innerHTML: texto vindo do CYDex (títulos de aviso,
// nomes de save) nunca vira HTML.

type Child = Node | string | null | undefined | false;
type Attrs = Record<string, string | number | boolean | EventListener | undefined>;

export function el<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  attrs: Attrs = {},
  ...children: Child[]
): HTMLElementTagNameMap[K] {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (v === undefined || v === false) continue;
    if (k.startsWith("on") && typeof v === "function") {
      node.addEventListener(k.slice(2), v as EventListener);
    } else if (k === "class") {
      node.className = String(v);
    } else if (v === true) {
      node.setAttribute(k, "");
    } else {
      node.setAttribute(k, String(v));
    }
  }
  for (const c of children) {
    if (c === null || c === undefined || c === false) continue;
    node.append(typeof c === "string" ? document.createTextNode(c) : c);
  }
  return node;
}

const SVG = "http://www.w3.org/2000/svg";

export function svg(tag: string, attrs: Record<string, string | number> = {}, ...children: Node[]): SVGElement {
  const node = document.createElementNS(SVG, tag);
  for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, String(v));
  node.append(...children);
  return node;
}

let toastTimer = 0;

export function toast(text: string, error = false) {
  const box = document.getElementById("toast")!;
  box.textContent = text;
  box.className = error ? "toast error" : "toast";
  box.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => (box.hidden = true), error ? 6000 : 3000);
}

/// Desliga o botão enquanto a promessa roda (evita clique duplo no CYD, que
/// atende um cliente por vez).
export async function busy<T>(btn: HTMLButtonElement, work: () => Promise<T>): Promise<T | undefined> {
  btn.disabled = true;
  try {
    return await work();
  } finally {
    btn.disabled = false;
  }
}
