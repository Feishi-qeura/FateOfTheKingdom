# BlueprintQueryAnalysis React Flow 视图设计

## 目标

将 BlueprintQueryAnalysis 中央图视图从手写 DOM/SVG 页面替换为真实的 React Flow 前端，并继续由 Unreal Engine `SWebBrowser` 离线承载。静态依赖扫描和运行流程采集沿用现有 C++ 数据源；前端负责节点、连线、箭头、发光与动画表现。

本次同时修复“图数据已生成，正在初始化图视图”长期不消失的问题，使浏览器加载、前端就绪、数据同步和渲染完成都有明确状态与错误信息。

## 范围

### 包含

- 在 `Private/Dependencylibraries/ReactFlow` 中维护 Vite、React 和 `@xyflow/react` 前端源码。
- 将第三方依赖版本固定在 lockfile 中，构建产物输出到 `dist`，编辑器运行时不访问 CDN 或外网。
- 使用 React Flow 自定义节点和自定义边渲染静态依赖图及运行流程图。
- 保留节点单击选择、双击打开资产或进入文件夹、滚轮缩放、拖拽平移和适配视口。
- 修复 C++ 与页面之间的 ready、graph、selection、loading、rendered 和 error 状态同步。
- 增加 WebBrowser 加载错误、JavaScript 控制台消息、超时和资源路径日志。

### 不包含

- 改写静态依赖分析器、运行采集器或图数据模型。
- 引入远程服务、CDN 或编辑器内开发服务器。
- 实现尚未完成的蓝图类内 Ubergraph/FunctionGraph 解析。
- 提供任意主题编辑器；颜色和动画先由集中式样式常量控制。

## 架构

系统分为三个边界清晰的单元：

1. **C++ 图数据与宿主层**
   - `SBlueprintQueryAnalysisWidget` 继续触发扫描和采集。
   - `SBlueprintQueryAnalysisGraphView` 只负责创建 WebBrowser、加载本地入口、维护同步状态和注入 JSON。
   - 宿主仅在页面明确报告 ready 后推送图数据；只有页面报告 rendered 后才关闭 Slate 初始化遮罩。

2. **React Flow 应用层**
   - 接收现有 `FBPQAGraphExporter::ExportToJson` 输出。
   - 将 BPQA 节点和边映射为 React Flow nodes/edges。
   - 根据视图模式执行静态依赖布局或运行时泳道布局。
   - 通过稳定的 `window.BPQASetGraph`、`window.BPQASetSelected` 和 `window.BPQASetLoading` 接口接收宿主调用。

3. **离线构建产物层**
   - Vite 使用相对资源路径生成 `dist/index.html` 和本地 assets。
   - Unreal 加载磁盘上的 `dist/index.html`；所有脚本与样式都来自插件目录。
   - C++ 不依赖 Node.js，Node.js 仅用于开发和重新构建前端。

## 数据流与握手

1. Slate 创建 `SWebBrowser` 并加载本地 `dist/index.html`。
2. WebBrowser 的 load started/completed/error 与 console 回调写入专用日志。
3. React 应用挂载并安装宿主 API 后，发出 `bpqa://ready`。
4. C++ 收到 ready，推送当前 loading 状态、graph JSON 和 selection。
5. React 完成节点/边映射、布局和首帧渲染后，发出 `bpqa://rendered/<node-count>`。
6. C++ 收到 rendered 后清除 `bGraphPresentationPending`，Slate 遮罩才折叠。
7. 若页面加载失败、脚本异常或在限定重试周期内未 ready/rendered，遮罩改为错误状态，并显示本地路径和日志定位提示，避免无限等待。

为避免 ready 事件因自定义协议差异丢失，C++ 的 load-completed 重试仍保留，但每次推送都以页面 API 已存在为前提；状态机不会在 JavaScript 实际未执行时误判成功。

## 节点视觉规则

节点采用 React Flow 自定义 node type，颜色判断集中在一个样式映射函数中：

- **未连接节点**：灰色边框、灰黑背景、低饱和文字，不显示彩色外发光。
- **已连接蓝图节点**：蓝色边框和蓝色外发光；蓝图判定优先使用数据中的 `isBlueprint`，并兼容包含 Blueprint 的类型名。
- **已连接其他资产节点**：统一使用青绿色边框和青绿色外发光，不再按各类非蓝图资产分裂配色。
- **选中节点**：叠加金色高亮描边，但不覆盖节点本身的连接状态颜色。
- **运行流程节点**：遵循同一连接状态规则，并允许更紧凑的横向尺寸。

节点标签、类型和资产路径均进行长度限制与安全转义；完整信息仍由右侧 Slate 详情面板展示。

## 连线、箭头、发光与流水动画

使用 React Flow 自定义 edge type，而不是只修改默认 CSS：

- 主路径使用 SVG Bézier 路径，并带同色箭头 marker。
- 底层绘制较宽、低透明度的 glow path，通过 SVG filter 产生柔和外发光。
- 上层绘制白色高亮流光 path，使用 `stroke-dasharray` 与 `stroke-dashoffset` 循环动画形成流水线效果。
- 白色流光沿主路径方向移动，并延伸至箭头附近；箭头保持清晰，不被 glow 淹没。
- 静态依赖边沿用资产依赖主题色；运行时顺序边可使用偏蓝主题色，但白色流光规则一致。
- 动画只修改 SVG stroke offset，不触发布局，避免持续重排。

所有颜色、线宽、发光强度、虚线长度和动画速度集中在样式常量中，后续修改外观无需改动数据桥。

## 布局与交互

- 静态依赖图按依赖深度从左到右布局；没有边的节点放入独立灰色区域。
- 运行流程图按事件类型分泳道，并按时间顺序从左到右排列。
- 数据更新后调用 React Flow 的 fit view；后续选择更新不重复重置用户视口。
- 单击节点发送 select，双击节点发送 open；节点 ID 使用 URL-safe Base64 编码，保持现有 C++ 解码契约。
- React Flow 内建缩放、平移和控件行为替换现有手写 pointer/wheel 逻辑。

## 本地资源与插件配置

- 前端源码、`package.json`、lockfile 和构建配置均存放在 `Private/Dependencylibraries/ReactFlow`。
- `dist` 构建结果随插件保存，保证没有 Node.js 的编辑器机器也能使用。
- Vite 的 base 配置必须为相对路径，避免 `file:///` 环境请求磁盘根目录下的 `/assets`。
- BlueprintQueryAnalysis 插件显式声明其 WebBrowser 依赖关系；C++ 启动时检查 HTML 文件存在性以及 WebBrowser 可用性。
- 不同时使用 `InitialURL` 与 `ContentsToLoad` 两条互相竞争的初始加载路径；选用单一、可诊断的本地加载方式。

## 错误处理与诊断

- HTML 缺失：不创建假成功状态，Slate 显示缺失的绝对路径。
- WebBrowser/CEF 不可用：显示模块不可用提示，并记录引擎 Browser 配置状态。
- 页面加载错误：处理 `OnLoadError`，停止正常同步重试并显示失败状态。
- JavaScript 异常：接入 `OnConsoleMessage`，记录 source、line、severity 和 message。
- ready 超时：显示“前端未就绪”，记录当前 URL、HTML 路径和重试次数。
- rendered 超时：显示“图数据已发送但前端未确认渲染”，保留图节点/边数量便于定位。
- 空图不是错误：根据静态、运行或节点流程模式显示对应空状态。

## 测试与验收

### 前端自动测试

- 图数据到 React Flow nodes/edges 的映射。
- 蓝图、普通资产、未连接和选中状态的样式分类。
- 自定义边包含主线、箭头、glow path 和白色流光 path。
- ready、graph、selection、loading、rendered 接口行为。
- Vite 生产构建只引用相对本地资源，不包含 CDN URL。

### C++/集成验证

- 对同步状态机的 ready 前不推送、ready 后推送、rendered 后隐藏遮罩、error/timeout 显示错误进行自动化或可独立调用测试。
- 编译 BlueprintQueryAnalysis 编辑器模块。
- 在编辑器打开 Blueprint Insight，执行静态扫描，确认节点和边显示且遮罩消失。
- 打开运行流程视图，在空数据时显示正确空状态；采集后显示运行节点与流水边。
- 验证单击详情、双击资产/文件夹、缩放和平移。
- 断开某节点的全部边，确认灰色；连接蓝图确认蓝光；连接普通资产确认统一青绿光。
- 断开或重命名前端入口进行一次错误路径验证，确认不再无限停留在初始化提示。

## 完成标准

- `dist` 由真实 React、React Flow 和 Vite 生成，而非手写 DOM/SVG 图编辑器。
- 编辑器运行时完全离线，网络面板和产物检索均无远程依赖。
- “扫描静态依赖”和“运行流程视图”不会无限停留在初始化遮罩。
- 节点、线段、箭头、着色、SVG 发光及白色流水动画符合本设计规则。
- 前端测试、生产构建和 Unreal 编辑器模块编译通过；编辑器内关键交互完成验证。
