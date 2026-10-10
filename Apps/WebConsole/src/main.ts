import { createApp } from "vue";
import App from "./App.vue";

// 创建 Vue（前端框架）根应用并挂载到 index.html 的 #app 容器。
// 当前入口保持最小化，路由、状态管理和全局错误边界将在对应基础模块中统一注册。
createApp(App).mount("#app");
