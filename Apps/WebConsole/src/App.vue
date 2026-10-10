<template>
  <!-- 当前页面仅作为 WebConsole（管理控制台）工程骨架，后续业务模块通过路由分包加载。 -->
  <main class="page">
    <h1>VisionAIPlatform</h1>
    <p>机器视觉 AI 智能监管平台管理控制台</p>

    <section class="cards">
      <article>
        <strong>设备中心</strong>
        <span>摄像机、边缘节点、视频流状态</span>
      </article>
      <article>
        <strong>算法中心</strong>
        <span>算法包、模型包、Pipeline（算法管线）</span>
      </article>
      <article>
        <strong>事件中心</strong>
        <span>候选事件、人工复核、证据链</span>
      </article>
    </section>
  </main>
</template>

<style scoped>
/* 页面容器采用受控最大宽度，避免超宽显示器上内容过度拉伸。 */
.page {
  max-width: 1100px;
  margin: 0 auto;
  padding: 32px;
  font-family: Inter, "Microsoft YaHei", sans-serif;
}

/* 三个入口卡片仅用于工程占位；真实控制台后续使用统一布局和响应式断点。 */
.cards {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 16px;
  margin-top: 24px;
}
article {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 20px;
  border: 1px solid #ddd;
  border-radius: 10px;
}
span {
  font-size: 13px;
  color: #666;
}
</style>
