# GlesGpu 原始单 EFB 分支 VRAM 命令正确性改进方案

状态：M1/M2/M3/M4-A 已通过 Wii 实机回归

日期：2026-09-28

目标基线：`main` / `77b4dd3`

经验来源：`feature/wiisxrx-s7-vram-sync` / `a744d5e`

## 1. 目的

S7 左右分区 VRAM 实验提高了兼容性，但其额外的 EFB/backing 保存、恢复和合成会降低 Wii 上的效率，并改变原始分支的渲染分辨率路径。本方案不把 S7 整体移植到 `main`，而是提取其中已经验证的命令语义、区域分析和同步规律，在保留原始单 EFB 渲染架构的前提下提高正确性。

本方案首先解决以下问题：

- GP0 A0h（CPU 到 VRAM）、C0h（VRAM 到 CPU）、80h（VRAM 到 VRAM）尺寸和环绕处理不完整。
- 80h 源、目标重叠以及 E6 Mask Bit 处理不完整。
- GP0 02h Fill 跨越 VRAM 右边界或下边界时，CPU 镜像被截断。
- A0/80h/02h 更新了 `psxVuw` 后，显示页的提交时机依赖零散游戏补丁。
- Draw 命令的实际 VRAM 写入区域缺少统一计算，导致页面选择和同步判断依赖坐标猜测。
- GX 中的新内容被 Move、C0 或纹理解码读取时，旧的 CPU VRAM 可能被使用；反过来，旧的异步快照也可能覆盖较新的 A0 数据。

## 2. 目标与非目标

### 2.1 目标

1. 保持 `main` 当前的输出分辨率、viewport、EFB 尺寸和显示流程。
2. 为 A0/C0/80h/02h 建立统一、可单元测试的 PS1 VRAM 区域规则。
3. 只在真实读写依赖出现时执行上传、GX 同步或读回。
4. 用统一 Draw footprint 代替分散的坐标特判，逐步减少游戏专用补丁。
5. 功能关闭时与当前 `main` 完全一致，并允许逐阶段 A/B 测试。
6. 优先修复可以在单 EFB 架构中正确实现的问题，不以牺牲普遍性能换取少数场景正确性。

### 2.2 非目标

1. 不移植 S7 的 640+384 左右 tile backing、tile 切换和最终拼接显示。
2. 不为每条 Draw 命令保存完整 1024x512 GPU VRAM。
3. 第一阶段不承诺任意纹理、多边形、半透明图元在当前 EFB 映射之外仍可被完整保存。
4. 第一阶段不重写半透明 ABR、Dithering、Texture Window 或 RGB24/MDEC 路径。
5. 不立即删除 FF7、FF9、放浪冒险谭、恐龙危机等现有补丁；只有通用路径通过 A/B 回归后才逐个移除。

## 3. 架构边界

PS1 逻辑 VRAM 固定为 1024x512、每像素 16 位。原始 GlesGpu 同时存在三类内容：

- `psxVuw`：CPU 可访问的完整 1024x512 镜像。
- GX EFB：当前被映射和渲染的显示区域，通常只代表某一个页面。
- 纹理缓存及 EFB 快照：由 `psxVuw` 或 GX 内容派生的数据。

单 EFB 架构不能同时无损保存所有物理 VRAM 页面。因此本方案区分两种保证：

- **命令语义保证**：A0/C0/80h/02h 在 `psxVuw` 中的尺寸、环绕、重叠和 Mask 行为必须正确。
- **GPU 页面保证**：只有能够确定 EFB 映射和内容所有权的区域，才能通用保证 GX 与 `psxVuw` 同步。

区域分析可以证明一条命令触及哪里，但不能凭空保存 EFB 之外的复杂 GPU 绘制结果。遇到后一种情况，应保留页面选择补丁，或明确进入另一项架构设计，不得在本项目中暗中重新引入 S7 backing。

## 4. 基本数据约定

### 4.1 半开矩形

所有内部矩形统一采用半开区间：

```text
[x, x + width) × [y, y + height)
```

这样相邻区域不会被误判为重叠，宽高也不需要混用 `-1`。只有调用保留旧接口的 `InvalidateTextureArea()` 等函数时，才在适配层转换其参数约定。

### 4.2 坐标和尺寸

- VRAM X 坐标使用低 10 位，范围 0..1023。
- VRAM Y 坐标使用低 9 位，范围 0..511。
- A0/C0/80h 的宽度字段为 0 时表示 1024。
- A0/C0/80h 的高度字段为 0 时表示 512。
- 02h Fill 的零宽或零高仍表示空操作，不能套用传输命令的零尺寸规则。
- 区域越过 X=1024 或 Y=512 时，拆成最多四个不环绕矩形，并保留每块相对原命令的 X/Y 偏移。

### 4.3 写入序列

沿用当前 readback 代码中的单调 `write sequence` 思路：

- CPU 命令完成时记录 CPU 写入序列。
- GX Draw 提交时记录 EFB 写入序列。
- 快照记录捕获时所代表的 GX 序列。
- 合并或读回时只允许较新的生产者更新较旧的副本。

序列比较是正确性条件，不只是诊断信息。

## 5. 必须保持的不变量

1. **最新写入者优先**：旧 EFB 快照不得覆盖序列更高的 A0、80h 或 02h CPU 写入。
2. **读取前同步**：Move、C0 或纹理解码读取某区域前，该区域的 CPU 镜像必须包含所有更晚的 GX 写入。
3. **绘制前提交**：一条 GX Draw 依赖或覆盖待上传 CPU 区域时，较早的 CPU 写入必须先提交到正确 EFB 页面。
4. **显示前提交**：GP1 选择的显示区域存在 CPU-new 数据时，必须在 presentation 前提交。
5. **只同步依赖区域**：普通 Draw、Fill 和 A0 不得因为本功能无条件执行 `GX_DrawDone()` 或完整 EFB copy。
6. **显示与所有权分离**：OSD、黑边、FPS 等 presentation 内容不得写回 PS1 VRAM。
7. **环绕始终显式拆分**：不能用一个跨边界 bounding box 表示两个物理上分离的 VRAM 区域。
8. **原始路径可恢复**：功能宏关闭时，不改变命令调度、显示尺寸或原有补丁行为。

## 6. GP0 命令语义

### 6.1 A0h：CPU 到 VRAM

实施规则：

1. 使用统一传输尺寸解码，包含 0→1024/512。
2. 输入像素按命令的逻辑行列前进，X/Y 分别在 1024/512 环绕。
3. 应用当前 E6 状态：
   - `set mask`：成功写入的像素置 bit15。
   - `check mask`：目标像素 bit15 已设置时跳过写入。
4. 完成后仅对实际成功写入的环绕区域进行纹理失效和 CPU 序列更新。
5. 与当前/前一显示页相交的 CPU-new 区域进入 pending upload 集合；不立即强制换帧或等待 GX。
6. RGB24/MDEC 暂时保留原有路径，但共用安全的尺寸解码。不得让 RGB15 的 pending 逻辑抑制 `PrepareRGB24Upload()`。

注意：当前 `main` 的直接写入路径没有完整实现 E6 Mask 规则，不能只修 `primLoadImage()` 的头部而保留原像素循环。

### 6.2 C0h：VRAM 到 CPU

实施规则：

1. 使用统一传输尺寸解码。
2. 在输出第一个字之前调用 `EnsureCpuCurrent(readRect)`。
3. 只解析与读取区域相交且 GX 序列较新的 16x16 ownership block。
4. 必须支持逐像素 X/Y 环绕；奇数像素数量仍遵守现有 GPU read ABI，但不能多消费最后一个像素。
5. 同一个 GX generation 已同步的 block 不得重复等待或重复合并。

### 6.3 80h：VRAM 到 VRAM

实施规则：

1. 使用统一传输尺寸解码。
2. 复制前调用 `EnsureCpuCurrent(sourceRect)`；只有源区域确实含有 CPU 端未拥有的较新 GX 数据时，才允许同步读回。
3. 应用当前 E6 状态：
   - `set mask`：成功写入目标的像素置 bit15。
   - `check mask`：目标像素 bit15 已设置时保持不变。
4. 支持源和目标分别独立环绕。
5. 重叠复制必须采用 PS1/DuckStation 已验证的复制顺序。当前 DuckStation 对水平重叠按源、目标关系选择正向或反向列复制，行保持正向。S7 的“先把整个源区域复制到 scratch，再统一写目标”虽然避免了自污染，但不应未经硬件语义验证直接移植，因为它可能改变重叠命令结果。
6. 目标实际写入区域进行纹理失效、CPU 序列更新，并在需要显示时加入 pending upload。
7. 源、目标相同但 `set mask` 开启时不能直接跳过，因为命令仍需更新 bit15。

第一版实现优先与当前 DuckStation 软件光栅器的可观察行为一致，并为正向、反向、Mask、水平环绕、垂直环绕分别建立测试。

### 6.4 02h：Fill VRAM

实施规则：

1. X 起点按 16 像素向下对齐：`x & 0x3f0`。
2. 宽度取低 10 位后按 16 像素向上对齐。
3. Y 和高度取低 9 位。
4. 零宽或零高为空操作。
5. Fill 不受 DrawArea 限制，也不应用 E6 的 set/check mask 规则。
6. X/Y 越界按 VRAM 尺寸环绕，CPU 镜像、纹理失效和 pending upload 都按拆分块处理。
7. 现有 interlaced Fill 行为单独保留并建立回归，不在第一版顺便重写。

## 7. Draw 命令写入区域

从 S7 提取纯 CPU 的 command footprint planner，但不提取 tile span 和 GX backing 逻辑。planner 至少支持：

- 20h..3Fh 三角形、四边形及纹理/渐变变体。
- 40h..5Fh 线段和折线，包括终止字判断。
- 60h..7Fh 可变、1x1、8x8、16x16 Rectangle/Sprite。
- DrawOffset。
- DrawArea 的包含端点语义，并在内部转换为半开矩形。
- `offsetline()` 实际生成的窄四边形额外 footprint。
- 0..1024、0..512 的最终裁剪。

planner 只输出：

```text
command write rectangle
intersects active EFB map
intersects previous/current display page
intersects pending CPU upload
is wholly outside representable EFB map
```

它不得改变 vertex、viewport、projection 或分辨率。

### 7.1 单 EFB 下的处理级别

- **A：完全位于当前 EFB 映射内**：使用原始 GX Draw 路径。
- **B：与当前 EFB 部分相交**：GX 只负责可表示部分；CPU 镜像补偿仅限已经证明等价的命令类型。
- **C：完全位于当前 EFB 外，但属于可软件镜像的纯色不透明 Fill/Tile**：允许更新 `psxVuw`。
- **D：完全位于当前 EFB 外的纹理、半透明、Mask、多边形或线**：第一阶段不宣称通用正确；保留页面选择或游戏补丁。

不得把 planner 的“知道写在哪里”误解为“已经保存了绘制结果”。

## 8. Pending CPU Upload 调度

### 8.1 数据结构

维护一个小型矩形集合，而不是单个不断扩大的 `xrUploadArea`：

- 相交或相邻且属于同一连续物理区域的矩形可以合并。
- 跨 VRAM 边界的两块不得合并成覆盖整个 VRAM 的 bounding box。
- 集合达到上限时，提交最早且可映射的项；不能通过扩大 bounding box 把中间较新的 GX 内容覆盖掉。
- 每项记录 CPU 写入序列和所属显示映射。

### 8.2 命令前屏障

在完整 GP0 packet 即将执行、但尚未提交 GX Draw 时检查：

1. Draw footprint 与 pending 区域相交：先上传较早的 pending 交集，再执行 Draw。
2. 半透明或 check-mask Draw 读取目标背景时，必须把目标范围内较早的 pending 数据先上传。
3. Move 的源读取 `psxVuw`；如果源是 CPU-new pending，它已经可直接读取，不需要为了 Move 先上传到 EFB。
4. Move/Fill/A0 产生的新 CPU 目标区域按命令完成顺序重新登记。
5. 完全不相交的命令不触发 UploadScreen、EFB copy 或 GX wait。

### 8.3 显示屏障

以下时点提交与即将显示页面相交的 pending 区域：

- GP1 display start/page 发生变化。
- `updateDisplayGl()`/`flipEGL()` 将呈现该页之前。
- 显示从 disabled 变为 enabled。

上传必须发生在 OSD、FPS 和黑边绘制之前，且这些 presentation 内容不能改变 VRAM ownership。

## 9. GX 到 CPU 的按需读屏障

现有 `gpuVramReadback.inc` 已有 16x16 tile 序列、映射分类和快照机制，应在其上收敛为一个入口：

```text
EnsureCpuCurrent(rect, reason)
```

调用者只包括：

- C0h 读取。
- 80h 源读取。
- 直接从 `psxVuw` 解码、但源区域可能由 GX 更新的纹理或 CLUT。

处理顺序：

1. 拆分环绕区域。
2. 比较相交 block 的 CPU/GX sequence。
3. 没有 GPU-new block 时立即返回，不能调用 GX。
4. 有依赖时只捕获/合并所需区域；同一 generation 最多等待一次。
5. 完成时再次比较 captured sequence。捕获期间若 A0 写入了更高序列，旧快照不得覆盖该 block。
6. 只对实际改变的 CPU 区域失效纹理缓存。

不能把当前只为 DC1/DC2 使用的 Move readback 条件简单改成“所有 80h 都捕获”。此前日志已经证明，无条件捕获会用旧 EFB 内容覆盖动画 A0 数据。

## 10. 性能约束

以下条件任一出现即视为实现方向错误：

- 普通帧的每条 Draw 都执行 `GX_DrawDone()`。
- 没有 C0/Move/纹理读依赖的帧仍进行 EFB→CPU copy。
- A0、Fill 或普通不透明 Draw 触发完整 640x480 EFB copy。
- 每条命令扫描全部 64x32 ownership blocks；只允许遍历与矩形相交的 block。
- 为了区域判断改变 viewport、EFB 高度、输出分辨率或 presentation 缩放。
- pending 集合通过大 bounding box 上传 CPU 旧数据，覆盖其间较新的 GX 内容。
- 功能关闭后仍改变原始分支行为。

允许的成本：

- 每个 GP0 packet 的常数级坐标解析。
- 仅遍历 footprint 覆盖的少量 16x16 block。
- 80h 本身所需的 CPU 像素复制。
- 存在真实 GPU→CPU 依赖时的一次区域捕获和等待。

性能结论以 Wii 实机 Release 为准；Debug 写 SD Log 的速度和音频卡顿不能作为最终结论。

## 11. 分阶段实施

### M0：测试和纯区域工具

- 新增独立 rect/transfer helper，不引用 GX。
- 新增 host tests。
- 功能尚不接入 GlesGpu 命令路径。

验收：尺寸解码、环绕拆分和区域相交测试全部通过。

实施记录：

- `GlesGpu/gpuVramCommandRect.h/.c`：传输尺寸、Fill 解码、环绕拆分、重叠和交集。
- `host_tests/test_gpu_vram_command_rect.c`：固定用例及 20,000 组确定性随机性质测试。
- 严格警告构建和 Address/UndefinedBehavior Sanitizer 测试均通过。

### M1：A0/C0/80h/02h CPU 语义

- 接入尺寸解码。
- 修复 A0/C0 的逐像素环绕。
- 修复 A0/80h 的 E6 Mask 行为。
- 修复 80h 重叠复制顺序。
- 修复 Fill CPU 镜像环绕和分块纹理失效。

验收：功能关闭与 `main` 一致；功能开启通过命令级测试，尚不移除任何游戏补丁。

实施记录：

- `GlesGpu/gpuVramCommandTransfer.h/.c` 使用字节访问固定 PS1 VRAM 的
  little-endian 像素布局，避免 Wii 大端主机直接解引用 `uint16_t` 产生歧义。
- A0/C0 按传输坐标逐像素环绕；宽/高字段为 0 时分别解码为 1024/512；
  C0 奇数像素传输的最后一个返回字高 16 位补 0。
- A0 和 80h 执行 E6 的 set-mask/check-mask；80h 采用已验证的横向重叠
  复制方向、纵向正序及双轴环绕规则。
- 02h Fill 始终同步 CPU VRAM 镜像，并将跨边界目标拆成最多四块执行
  纹理缓存失效；保留原有单 EFB 绘制和输出分辨率路径。
- 所有运行时修改受 `GLES_VRAM_COMMAND_FIXES` 控制；关闭时保留原始路径。
- `host_tests/test_gpu_vram_command_transfer.c` 覆盖 endian、E6 mask、Fill
  环绕、C0 奇数返回、80h 重叠/环绕及同源目标 set-mask；另以 64 组
  确定性随机数据对 Move 的方向、双轴环绕和 Mask 组合做差分验证。
- 严格警告、Address/UndefinedBehavior Sanitizer、Wii Debug（开/关）和
  Wii Release（开启）构建均通过。
- Wii 实机确认没有新增问题，原有游戏补丁仍正常工作；M1 暂不移除补丁。

### M2：Draw footprint planner

- 解析 20h..7Fh。
- 加入 DrawOffset、DrawArea 和 line footprint。
- 只输出分类和计数，暂不改变渲染。

验收：用合成 packet 覆盖所有命令族；对现有游戏仅做低量聚合统计。

实施记录：

- `GlesGpu/gpuDrawFootprint.h/.c` 从 little-endian GP0 packet 独立计算
  半开写入区域，不引用 GX，也不修改顶点、viewport 或显示状态。
- 覆盖 20h..3Fh 的 Flat/Textured/Gouraud 三角形和四边形，按 PS1
  的两个三角形分别执行超大 primitive 剔除后合并 footprint。
- 覆盖固定线和 Flat/Gouraud polyline，使用真实 packet 字数和终止字；
  footprint 包含 `offsetline()` 窄 GX 四边形可能接触的额外边缘像素。
- 覆盖可变、1x1、8x8、16x16 Rectangle/Sprite；Rectangle 在加
  DrawOffset 后执行 11-bit 坐标截断。
- 统一应用 DrawOffset、DrawArea 包含端点语义及 1024x512 VRAM 裁剪，
  区分有效、部分裁剪、完全区域外、PS1 预裁剪丢弃和 malformed packet。
- GP0 调度器额外保存 polyline 的真实 packet 字数。planner 在旧绘制函数
  之前运行，只累计命令分类和与 active map、前后显示页、旧 pending
  upload 的相交计数；默认不写逐命令 Log，不执行上传、读回或 GX wait。
- 自查时发现 opcode 范围不能单独代表实际绘制：正常表的 6Ch..6Fh 指向
  `primNI`，跳帧表又把 Draw 指向 `primNI` 或仅消费 polyline 的 Skip
  handler。运行时现按所选 dispatch handler 过滤，避免 M3 为未提交 GX
  的命令建立错误屏障；纯 planner 仍保留 nominal opcode 语义用于测试。
- `host_tests/test_gpu_draw_footprint.c` 覆盖全部 polygon 布局、固定线、
  两类折线、四类 rectangle、DrawOffset、DrawArea、区域外、终止字和
  endian，并覆盖四边形单独三角剔除和 DrawArea 最右下端点；严格警告及
  Address/UndefinedBehavior Sanitizer 均通过。
- Wii Debug（开/关）和 Wii Release（开启）构建通过；M2 实机主要验证
  planner 的 CPU 成本，画面应与 M1 相同。

### M3：精确 pending upload

- A0/80h/02h 写入登记 pending 矩形。
- Draw 前按 footprint 检查并提交依赖交集。
- presentation 前提交即将显示页面。
- 替代“直到换帧才处理”的单矩形行为。

验收：放浪冒险谭两个 Loading、FF9 菜单及普通 A0 动画不回退；无依赖帧没有新增 GX wait。

实施记录：

- 新增 `GlesGpu/gpuPendingUpload.h/.c`，最多保存 32 个半开物理 VRAM
  矩形；跨边界写入先拆成最多四块。只有合并后仍是无空洞矩形时才合并，
  不再用覆盖中间无关区域的大 bounding box。
- 支持按写入顺序查找依赖、从集合精确扣除已提交区域；一次扣除最多产生
  上、下、左、右四个余区。Add/Subtract 都是事务式操作，容量不足不会留下
  半更新状态；极端溢出时显式恢复旧 `bNeedUploadAfter/xrUploadArea` 路径。
- A0 完成、80h Move 目标以及 02h Fill 的 CPU 镜像写入会登记 pending；
  已由 `UploadScreen()` 写入活动 EFB 的区域会在成功提交后扣除，Fill 已直接
  画入活动 EFB 的非环绕部分也会立即扣除。
- 每条实际提交 GX 的 Draw 在旧 handler 运行前，用 M2 footprint 检查 pending；
  只要相交，就按最早写入顺序上传该 pending 项位于活动 EFB map 内的部分。
  不相交 Draw 不调用 `UploadScreen()`，也不增加 `GX_DrawDone()`。
- `updateDisplayGl()` 在黑边、调试文字和 presentation 之前提交活动显示页，
  防止 presentation 内容污染后再上传。
- FF7、FF9 和 `AUTO_FIX_NO_SWAP_BUF` 游戏暂时继续走已验证的旧 deferred
  upload/page-routing 补丁；M5 A/B 验证时再逐项交给通用 M3，避免在本阶段
  同时改变页面选择和上传时序。RGB24 与现有隔行专用路径同样保持原逻辑。
- `UploadScreen()` 现有的单像素宽/高 GX 限制仍保留；M3 不会把未实际提交的
  单像素区域误标为 clean。后续若真实游戏命中该情况，需要单独实现安全上传，
  不能通过扩大区域覆盖邻近 GPU-new 像素。
- `host_tests/test_gpu_pending_upload.c` 覆盖无空洞合并、L 形不合并、双轴
  环绕、最早写入选择、中心扣除、容量事务性，以及 120 组随机 Add/Subtract
  像素覆盖差分。

### M4：通用 GPU→CPU 读屏障

- 统一 C0、Move 和纹理/CLUT 的 `EnsureCpuCurrent()`。
- 用 sequence 阻止陈旧快照覆盖新 A0。
- 对真实依赖合并等待。

验收：DC1 球体、DC2 水下/片头、已发现的 Move 动画场景正确；普通动画无小块纹理污染。

M4-A 实施记录（C0/Move/Mask target）：

- ownership sequence 跟踪与旧的逐帧 EFB snapshot 流程分离。功能开启时所有
  游戏都会维护 CPU/EFB tile 序列，但只有读取区域被证明存在 GPU-new 数据时
  才尝试捕获；没有依赖的 C0/Move 不调用 GX wait 或 EFB copy。
- 新增 `GlesGpu/gpuVramReadDependency.h`，以严格的 `EFB sequence >
  max(CPU write sequence, materialized sequence)` 判定真实依赖。相等表示 CPU
  已经拥有该 generation，旧 snapshot 不得再次覆盖。
- C0 在输出第一个字前、Move 在复制源前调用统一的
  `GlesGpuEnsureCpuCurrent()`；E6 check-mask 的 A0/Move 还会在读取目标 bit15
  前确保目标区域为当前内容。
- `UploadScreen()` 是 CPU→EFB 复制，不是新的 GPU 颜色生产者。上传后保留
  EFB full-coverage 证明，但 tile 序列回落到对应 CPU epoch；后续 GX Draw
  才会产生 GPU-new 依赖。
- 只有 EFB full-coverage tile 才允许写回 CPU。partial-only tile 会显式返回
  unresolved，绝不猜测并覆盖 `psxVuw`。完整同步的 16x16 tile 记录
  materialized sequence，同一 GX generation 的连续读取不会重复捕获；读取矩形
  边缘只覆盖部分 tile 时不提升整个 tile 的 epoch。
- DC2 的 delta-filtered Move 路径和 DC1 的 64x64 小区域捕获继续优先使用已验证
  的旧实现，避免通用 full-tile 合并重新产生方块；既有游戏补丁本阶段不删除。
- 旧的 frame-end snapshot 仍只由 `AUTO_FIX_VRAM_READBACK` 启用。M4-A 不会使
  普通游戏每帧读回 EFB；首次真实依赖目前仍使用既有完整活动 EFB snapshot，
  区域化 GX copy 留待确认正确性和实机成本后再优化。
- 纹理/CLUT 解码入口暂不接入，作为 M4-B 单独实施和回归，避免一次同时改变
  命令读取与纹理热路径。
- `host_tests/test_gpu_vram_read_dependency.c` 覆盖 CPU/EFB/materialized 严格
  顺序、full/partial coverage、陈旧 snapshot 拒绝以及 20,000 组确定性随机
  序列性质测试。
- 功能关闭的 Wii Debug DOL 与已验证 M3 关闭版逐字节一致（大小与 SHA-256
  均相同）。
- 首轮 Wii 回归发现 DC1/DC2“开始游戏/加载游戏”文字消失及 DC1 读取菜单
  花屏。先后仅绕过 M4 `EnsureCpuCurrent()`、以及完整恢复旧 ownership/snapshot
  边界，实机确认问题均不变。随后直接 A/B 已保存版本，确认 M2 正常、M3
  开始异常，排除了 M4 readback。
- 根因是两作的普通 MoveImage 已由旧 `UploadScreen(FALSE)` 按 previous display
  map 提交；M3 又登记了一个不携带 map 身份的 pending 项。previous map 不等于
  active map 时，完成钩子无法扣除该项，页面切换后陈旧区域被再次上传，覆盖
  新菜单。两作现与 FF7/FF9 等页面补丁一样排除在通用 M3 pending 管理之外，
  继续使用原 MoveImage 上传路径。
- M4 仍对两作保留旧 readback 边界：DC1 不启用通用 ownership，只使用精确
  64x64 feedback 捕获；DC2 保留 delta-filtered Move/C0、原 upload epoch 和
  `snapshot >= CPU` 判定；M4-A 不介入普通 Move 和 mask 目标。
- Wii 实机确认排除 M3 pending 重放后，DC1/DC2 的开始/加载菜单文字和 DC1
  读取菜单恢复正常；两作兼容修正通过回归。

### M5：旧补丁 A/B 收敛

- 一次只禁用一个旧补丁。
- Wii 实机 Release 和正确设置的 Dolphin 分别验证。
- 通用机制不能覆盖的页面外复杂 Draw 补丁继续保留，并记录原因。

## 12. Host tests 最低集合

### 12.1 尺寸和环绕

- A0/C0/80h `0,0` 解码为 1024x512。
- `(1000,500)+(40,20)` 拆成四块，面积总和保持 800。
- 负坐标规范化和双轴环绕。
- Fill X=1009、W=17：X 对齐为 1008，W 对齐为 32，并在右边界环绕。

### 12.2 A0/C0

- 水平、垂直和双轴环绕。
- 奇数像素传输不多消费最后一个像素。
- set-mask、check-mask 以及两者同时开启。

### 12.3 80h

- 不重叠复制。
- `src_x < dst_x` 的反向列复制。
- `src_x >= dst_x` 的正向列复制。
- 水平、垂直及源/目标不同位置的环绕。
- 源目标相同且 set-mask 开启。
- check-mask 保留目标 bit15 已设置的像素。

### 12.4 Draw footprint

- Flat/Textured/Gouraud 三角形和四边形。
- Flat/Gouraud line 与 polyline。
- 可变、1x1、8x8、16x16 rectangle。
- DrawOffset 后跨显示页。
- DrawArea 完全裁掉、部分裁剪和边界包含。
- line 末端实际 raster footprint。

### 12.5 顺序和所有权

- A0 pending → 相交 Draw：先上传再绘制。
- A0 pending → 不相交 Draw：不上传。
- A0 pending → page present：显示前上传。
- GX-new Move source：只同步源区域。
- 捕获开始后发生更高序列 A0：旧捕获不得回写。
- 同一 GX generation 被连续 Move/C0 读取：最多等待一次。

## 13. 游戏回归矩阵

第一轮必须覆盖：

- 放浪冒险谭：开机 Loading、开始游戏 Loading、RGB24 动画。
- FF9：动画、战斗菜单、页面切换。
- FF7：战斗菜单。
- 当前用于 F1/另一个游戏验证的小块 Move 动画。
- Rayman 2：高频 Move 场景及音频连续性。
- Dino Crisis 1：球体/扭曲反馈。
- Dino Crisis 2：片头、读取存档、水下人物和方块。
- MGS1：存档白字。
- Parasite Eve 2：红色警告背景。
- Alone in the Dark 4：门、闪电和人物清晰度。
- V8 Racing：右侧 VRAM 相关已知场景。

此外抽测至少 10 个没有专用补丁的普通游戏，确认画面、声音和速度没有回退。

判断顺序：

1. Wii 实机 Release 是正确性和性能主基准。
2. Dolphin 使用已确认的 EFB copy 设置复测。
3. 仅 Dolphin 异常时先判定模拟器设置或 EFB 模拟差异，不立即修改 Wii GPU 代码。
4. Release 可复现后才编译带有单一目标诊断的 Debug 版本。

## 14. 诊断原则

- 默认 Release 不写日志。
- Debug 只记录聚合计数：命令数、pending 命中数、区域上传数、GPU→CPU resolve 数和 wait 数。
- 只有聚合数据能明确缩小问题后，才启用单一场景的坐标/sequence 日志。
- 一次诊断应同时包含作出判断所需的 command、source/destination、mapping、sequence 和结果，避免多轮追加同类日志。
- 不在正常 Draw 热路径逐像素写 SD 日志。

## 15. 提交策略

- 使用独立功能宏，例如 `GLES_VRAM_COMMAND_FIXES`，初期默认关闭。
- M0..M5 每阶段单独提交，确保可以独立回退和 bisect。
- 纯 helper 和 host tests 先于运行时代码提交。
- 不把 Debug/Release build 目录、DOL、ELF、map 或测试日志提交到仓库。
- 旧游戏补丁的删除必须独立提交，不能与通用算法修改混在一起。

## 16. 完成标准

本项目完成不等于“所有 PS1 VRAM 页面都由原始单 EFB 完整模拟”。完成标准是：

1. A0/C0/80h/02h 的 CPU 可观察语义通过命令级测试。
2. pending upload 和 GPU→CPU 屏障由区域与 sequence 驱动，不再依靠无条件等待。
3. 已列回归游戏在 Wii Release 上不低于原始 `main` 的性能和分辨率。
4. 已被通用机制覆盖的旧补丁通过 A/B 后移除；无法覆盖的补丁有明确架构原因。
5. 没有把 S7 的 tile backing、低分辨率工作区或每命令保存/恢复带回原始分支。

## 17. 实现参考

实施时以行为和测试为依据，避免整段复制其他实现：

- 当前 S7 分支的 `gpuVramTilingRect.c`：尺寸解码和环绕矩形拆分经验。
- 当前 S7 分支的 `gpuVramTilingPlan.c`：Draw packet footprint 的命令族覆盖经验。
- 当前 `main` 的 `gpuVramReadback.inc`：EFB mapping、16x16 tile sequence 和快照所有权基础。
- 工作区 DuckStation `src/core/gpu_commands.cpp`：A0/C0/80h/02h packet 解码语义。
- 工作区 DuckStation `src/core/gpu_sw_rasterizer.inl`：Fill、A0 和 80h 的环绕、Mask 与重叠复制行为。

若 S7 的实验实现与 DuckStation 已验证行为冲突，以后者的命令语义为准；S7 仅提供同步问题和失败模式方面的经验。
