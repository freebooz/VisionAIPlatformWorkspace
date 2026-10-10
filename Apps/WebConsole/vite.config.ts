import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";

// Vite（前端构建工具）开发配置。
// 这里只保留框架插件和本地端口，生产反向代理、HTTPS 和安全响应头由 Deploy（部署层）负责。
export default defineConfig({
  plugins: [vue()],
  server: {
    port: 5173
  }
});
