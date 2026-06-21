import { defineConfig } from 'vitest/config';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';

export default defineConfig({
  base: './',
  plugins: [react(), viteSingleFile()],
  build: {
    // 内联所有 JS/CSS 到单个 index.html，使内联 module script 能在 UE 的
    // file:// 离线环境下执行（外部 module script 会被 Chromium 的 MIME/CORS 检查拦截）。
    target: 'chrome100',
    sourcemap: false,
    assetsInlineLimit: 100000000,
    cssCodeSplit: false,
  },
  test: {
    environment: 'jsdom',
    setupFiles: './src/test/setup.ts',
  },
});
