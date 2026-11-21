import { defineConfig } from "vite";

// Porta fixa: o tauri.conf.json aponta pra ela (devUrl).
export default defineConfig({
  clearScreen: false,
  server: { port: 1420, strictPort: true },
  build: { target: "es2021", outDir: "dist", emptyOutDir: true },
});
