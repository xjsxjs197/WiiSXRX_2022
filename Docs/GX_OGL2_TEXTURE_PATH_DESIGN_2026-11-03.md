# GX 类 OGL2 纹理路径设计（2026-11-03）

## 1. 目的

本文分析 Wii/GameCube GX 固定功能硬件上实现 Pete OGL2 纹理思路的可行性，并为 `GlesGpu`/`OpenGX` 后端给出可分阶段实施、可测量、可回退的改造方案。

目标不是移植 GLSL，而是在 GX 可用能力内实现相同结果：

- 普通纹理和 PS1 texture window 尽可能共用主纹理缓存；
- 降低 4-bit/8-bit CLUT 纹理的 CPU 展开和上传成本；
- 减少纹理更新造成的 `GX_DrawDone()` 全流水线等待；
- 保持 PS1 透明色、STP、半透明模式和 VRAM 失效语义正确；
- 所有新路径都必须可以独立关闭并回退到现有实现。

## 2. 现有实现

### 2.1 普通纹理

`SelectSubTextureS()` 根据纹理页、纹理模式、CLUT 内容、半透明状态和 UV 范围查找缓存。未命中时，CPU 从 PS1 VRAM 解码所需矩形，将颜色展开到 `texturepart`，再由 `glTexSubImage2D()` 写入一张 256×256 atlas。

主要结构：

- `pscSubtexStore[3][MAXTPAGES_MAX]`：逻辑缓存索引；
- `uiStexturePage[MAXSORTTEX_MAX]`：256×256 atlas 纹理对象；
- `pxSsubtexLeft[]`：atlas 空闲矩形；
- `texturepart`：CPU 侧临时展开缓冲。

OpenGX 最终将 RGBA 输入转换成 GX tiled `RGB5A3` 数据，并执行 data-cache flush。

### 2.2 Texture window

Texture window 当前使用独立缓存 `wcWndtexStore[]`。每个窗口被解码为一张独立纹理，并设置 `GL_REPEAT`，底层转换成 `GX_REPEAT`。

这种设计正确利用硬件 wrap，但存在以下成本：

- 不能与主 atlas 共享空间；
- 窗口组合多时缓存很快被轮转覆盖；
- 每次未命中都需要重新解码和写入完整窗口；
- `glTexImage2D()` 当前会执行 `GX_DrawDone()`；
- RRT4 等重度 texture-window 游戏可能频繁阻塞 CPU/GPU 流水线。

### 2.3 GX TMEM 缓存

OpenGX 另有 `texCacheRegionS[8]` 和 `texCacheUsedInfo[8]`，用于管理 GX 硬件 texture cache region。这与上述 PS1 内容缓存不是同一层，改造时不能混为一谈。

## 3. GX 上的类 OGL2 实现能力

### 3.1 Indirect texture 实现窗口坐标环绕

GX 没有 fragment shader，但 indirect texture stage 可以在普通 TEV 采样前修改纹理坐标。`GX_SetTevIndirect()` 支持先按 16、32、64、128 或 256 texel wrap 普通坐标，再叠加 indirect offset。

目标映射为：

```text
PS1 UV
  -> texture-window 局部坐标
  -> GX indirect wrap
  -> 加 atlas 缓存块起点
  -> 采样主 atlas
```

`GX_SetTevIndTile()` 是优先验证对象，因为它直接面向 tiled texturing。

限制：

- GX 没有直接的 8 texel indirect wrap；
- 8 texel 窗口需要通过坐标放大、`GX_ITW_16` 和后续缩放补偿，或继续使用旧路径；
- atlas 线性过滤必须增加 wrap gutter，避免跨块采样；
- indirect map 会占用 texture map/TMEM 资源；
- 必须验证负坐标、边界、sprite mirror 和半透明双 pass。

### 3.2 CI4/CI8 + TLUT 实现硬件 CLUT

GX 原生支持 `GX_TF_CI4`、`GX_TF_CI8`、`GX_InitTexObjCI()` 和 `GX_LoadTlut()`。理论数据量如下：

| PS1 模式 | 当前缓存格式 | GX CI 格式 | 索引数据量 |
|---|---|---|---:|
| 4-bit | RGB5A3，16 bit/texel | CI4 | 当前的 1/4 |
| 8-bit | RGB5A3，16 bit/texel | CI8 | 当前的 1/2 |
| 15-bit | RGB5A3 | RGB5A3 | 不变 |

收益包括：

- 同一索引纹理可以复用，CLUT 改变时只更新/切换 TLUT；
- 降低 CPU 调色板展开成本；
- 降低 backing memory、flush 和纹理读取带宽。

但 PS1 STP 不是普通 alpha。半透明 primitive 中必须区分透明、普通不透明和 STP 半透明纹素。CI/TLUT 路径仍可能需要两次绘制或 alpha compare，不能假设单 pass 自动得到正确结果。

## 4. 建议架构

### 4.1 保留两条可切换路径

```text
                    +----------------------+
PS1 textured draw ->| texture path selector|
                    +----------+-----------+
                               |
             +-----------------+-----------------+
             |                                   |
      legacy RGB5A3 path                 experimental GX path
   主 atlas + 独立 window cache      CI/TLUT + indirect window
```

新路径不得立即删除旧路径。建议增加编译期或运行期功能位：

- `GX_TEXPATH_PROFILE`：性能计数；
- `GX_TEXPATH_DEFER_SYNC`：纹理更新延迟同步；
- `GX_TEXPATH_INDIRECT_WINDOW`：indirect texture window；
- `GX_TEXPATH_CI_CLUT`：CI4/CI8 + TLUT。

每个功能位必须能独立关闭，以便定位兼容性问题。

### 4.2 Texture-window atlas 项

主缓存项需要扩展以下语义：

- 原始 PS1 texture-window 起点和尺寸；
- atlas 中实际内容起点；
- wrap gutter 尺寸；
- 过滤模式；
- indirect 参数或预计算的 GX wrap 枚举；
- 8 texel 窗口是否回退旧路径。

线性过滤布局建议：

```text
        bottom wrap row
     +-------------------+
right| window texels     |left
wrap |                   |wrap
col  +-------------------+col
         top wrap row
```

四角也应复制对应的对角纹素。

### 4.3 资源管理

Indirect 路径至少需要一个额外 texture map。当前 OpenGX 将最多八个预装载 cache region 当作 map id 使用，因此需要：

- 为 indirect map 保留固定 map/region；或
- 把预装载 region 与 TEV texmap id 解耦；或
- 仅在 window draw 时临时绑定并恢复普通路径状态。

不能在未审计 `checkLoadTextureObj()`、`SetRenderMode()` 和半透明双 pass 前直接占用一个槽位。

## 5. 分阶段实施

### Phase 0：基线观测

增加低开销计数：

- 普通缓存查找、命中、未命中；
- window 缓存查找、命中、未命中、轮转覆盖；
- 4/8/15-bit 解码 texel 数；
- `glTexImage2D()`/`glTexSubImage2D()` 调用次数和字节数；
- 因纹理更新执行的 `GX_DrawDone()` 次数；
- TMEM region 命中、空槽装入、全局清空次数。

计数只在开启功能位时编译，默认构建不增加热路径成本。

当前分支可用以下命令生成带计数的 GX 调试构建：

```sh
make GX_TEXPATH_PROFILE=1 GPU=GX Wii
```

每个呈现帧会在现有 `DISP_DEBUG` 批量日志中增加一行 `TDI GXTEX`，字段依次包含测试会话、CD ID、普通缓存、窗口缓存、三种纹理模式的解码 texel 数、上传调用/字节数、纹理上传导致的 `GX_DrawDone()` 次数，以及 TMEM 命中/重载/空槽装入/满缓存重置次数。保留 `TDI ` 前缀是为了通过当前 `TEXTURE_DIAG_ONLY` 文件过滤器。

检测到 CD ID、CD Label 或 GPU 纹理存储重新初始化时，日志会增加会话分隔：

```text
TDI GXTEX SESSION seq=1 cdid=SLUS00797 label="RIDGE RACER TYPE 4"
TDI GXTEX seq=1 cdid=SLUS00797 frame=123 ...
TDI GXTEX SESSION seq=2 cdid=SLUS00594 label="METAL GEAR SOLID"
TDI GXTEX seq=2 cdid=SLUS00594 frame=456 ...
```

因此多个游戏可以连续写入同一个 `debugLog.txt`，后处理工具可直接按 `seq` 或 `cdid` 分组，不需要逐个游戏导出日志。

#### Phase 0.2：miss 原因、atlas 占用与 MEM2

`debugLogEmu4.txt` 证明大量上传来自普通子纹理缓存 miss，而不是 8 个 GX
region 的容量。诊断版增加一条同帧 `TDI GXMISS`：

- `cause=noKey/uv/clut`：没有可复用 key、已有相同 key 但 UV 覆盖不足、相同
  CLUT 地址但 palette 内容指纹不同；三项之和应等于 `subMisses`；
- `inval=calls/entries`：PS1 VRAM 写入/绘制触发的失效调用和被失效条目；
- `gc=calls/entries`：atlas 垃圾回收次数和因此移除的条目；
- `bucket=`：单个 page/mode/hash bucket 达到容量上限的次数；
- `atlasclear=`：矩形分配失败后清空 atlas 索引的次数；
- `comp=calls/uploads/fullClear`：压缩次数、压缩过程重新上传次数及压缩失败全清；
- `atlas=used/backed/total`：有分配记录、已有 MEM2 backing、允许的 atlas 数量；
- `pages=active/max`：二维 page 指针表实际启用与编译期最大维度；
- `meta=valid/slots/capacity`：有效索引、包含空洞的已增长槽位、理论槽位；
- `mem2=used/free/minFreeKB`：当前 MEM2 已用、剩余及本游戏会话最低剩余。

当前 512 行 PS1 VRAM 只启用 32 个 texture page，因此
`pscSubtexStore[3][64]` 的后 32 个指针没有分配 backing；这是为 1024 行模式保留
的最大维度，不浪费 32 份实际块内存。实际分配为 `3×32×4096×16 = 6 MiB`。
每个 4096-entry 块又固定分成四个 1024-entry palette-hash bucket，内部使用率
由 `meta` 揭示。atlas backing 则按需分配，最多 64 张 256×256 RGB5A3，基础
数据约 8 MiB。

### Phase 1：降低无条件同步

当前纹理更新前无条件 `GX_DrawDone()`。第一阶段应确认真正需要等待的条件：CPU 是否即将覆盖仍被 GX 消费的 backing buffer。

候选方案：

- 每张 atlas 记录提交 generation/token；
- 只在覆盖 in-flight 纹理时等待；
- 对频繁更新的 atlas 使用双缓冲或延迟回收；
- 不再因任意纹理更新清空全部 `texCacheUsedInfo[]`。

这一阶段可能比 indirect path 本身带来更普遍的收益。

#### Phase 1A：8 个 TMEM region 的安全单槽替换

代码中的 8 个 `texCacheRegionS[]` 均为独立的 128 KiB GX texture cache
region，地址依次为 `0x00000` 至 `0xE0000`。它们没有固定的业务用途；普通
纹理和半透明纹理副本都会动态占用任意空槽。region 下标同时作为
`GX_TEXMAP0..7` 的 texture state register set 编号。

不能仅凭 CPU 侧 LRU 判断某个 region 已经可以覆盖，因为 GX 可能仍在异步
采样先前提交的纹理。Phase 1A 因此采用显式 GPU 栅栏：

1. 记录每个 draw 使用的 region，并维护 CPU 侧 LRU 序号；
2. 缓存满时用 `GX_SetDrawSync()` 给自上次检查以来使用过的 region 加 token；
3. 用 `GX_GetDrawSync()` 只选择 token 已经执行完成的 LRU region；
4. 如果 8 个 region 都无法证明空闲，回退到 `GX_DrawDone()`，不冒险覆盖；
5. 只替换选中的一个槽，不再清空其余 7 个有效映射；
6. Phase 1A 开启时取消 present 后的无条件 `resetTexCacheInfo()`，使映射可以
   跨帧复用；CPU 改写 backing texture 的路径仍保留原同步和全局重置。

该实现由编译开关控制，当前不会改变默认构建：

```sh
make GX_TEXPATH_PROFILE=1 GX_TEXPATH_PHASE1A=1 GPU=GX Wii
```

日志新增 `evict=single/fence/wait`：分别统计单槽替换、提交的 DrawSync 栅栏、
以及找不到已完成 region 时回退 `GX_DrawDone()` 的次数。Phase 1A 的首要验证
指标是 `wait/single` 比例及画面正确性；如果 `wait` 接近 `single`，说明 GPU
通常落后于 CPU，栅栏方案的收益有限，需要再调整替换或批处理策略。

调试版的 FPS/文字层会绕过 OpenGX，直接使用 `GX_TEXMAP0` 和
`texCacheRegionS[0]`。如果只取消帧末全局重置，CPU 记录仍指向游戏纹理，
而硬件 map 0 已被字体纹理覆盖，下一帧会把错误的硬件状态当作缓存命中，
造成纹理闪动。Phase 1A 在文字层结束后调用 `glResetCacheRegion()` 恢复原来
分配给 region 0 的普通或半透明游戏纹理，并把外部使用登记为 pending，纳入
下一次 DrawSync 栅栏。这样无需为 UI 预留槽位，8 个 region 仍全部供游戏使用。
日志中的 `restore=` 记录每帧执行该恢复的次数。

#### 失效来源与 atlas 碎片诊断（2026-11-07）

`debugLogEmu5.txt` 显示部分帧会固定失效并重新上传 13 个纹理块，且 atlas
空间耗尽时会清空全部元数据，形成数百次 miss 的尖峰。为区分“VRAM 内容真的
改变”和“atlas 分配器被碎片逼到全清”，性能构建新增两类逐帧日志：

- `TDI GXINVAL`：`order` 固定为
  `other,upload,move,fill,display,readback,draw`；`calls`、`entries`、`area`
  分别表示各来源的调用次数、实际失效的主缓存条目数、覆盖像素面积。
  `max=reason:entries@x,y+wxh` 记录本帧杀伤最大的单次失效。
- `TDI GXALLOC`：`bucket=max/nonempty/holes` 记录最大 bucket 深度、非空
  bucket 数和含空洞 bucket 数；`free=rects/dead/maxWxH` 记录 atlas 当前自由
  矩形、已耗尽矩形和最大自由矩形；`clearpre` 是全清发生前的同类快照，
  `request` 是当时无法放入的新块尺寸。

这些字段只做观测，不改变失效范围、atlas 分配或 8 个 TMEM region 的策略。

#### 普通 Tile 不再全局启用软件 VRAM 镜像（2026-11-07）

`debugLogEmu6.txt` 证明 99.98% 的实际缓存失效来自 `fill`，其中持续出现的
`0,0+512x240` 与 `0,240+512x240` 是双缓冲 framebuffer 的普通硬件 Tile。
历史检查表明，`TitleFillArea()` 原本由 `AUTO_FIX_NEED_SOFT_TITLE` 控制，只为
Brave Fencer Musashi、Vagrant Story 与 Pro Pinball: Timeshock 系列提供兼容性
软件镜像；后续修改注释掉了该开关和小 Tile 门槛，导致所有游戏把硬件 Tile
重复写入 `psxVuw`。

现在恢复原有游戏白名单和尺寸门槛。普通 Tile 只画入 EFB，不改 `psxVuw`，
因此无需失效纹理缓存；白名单游戏若确实执行软件写回，仍必须对实际写入区域
调用 `InvalidateTextureAreaTagged()`。真正的 GP0 block fill 仍按原逻辑写 VRAM
并做区域失效，不能省略。

`debugLogEmu7.txt` 中仍然每帧出现一次 512×240 `fill`，因此诊断继续细分：

- `GXINVAL order` 将 `fill` 拆为 `blockfill,titlefill`；
- `blockmode`、`titlemode` 分别统计失效条目的 4/8/16-bit texture mode；
- `reuse` 统计本帧 miss 是否与刚失效条目的 page、CLUT key 和区域重叠，
  用来判断写后是否立即重传；
- `TDI GXFILL kind=block` 记录 GP0 原始参数、规范化矩形、
  `clearCurrent/clearNext`、是否实际 CPU 写入 VRAM、EFB context 和颜色；
- `TDI GXFILL kind=title` 只会在白名单软件镜像真正执行时出现，并记录
  `dwActFixes`。

#### Phase 1B：atlas 分配失败改为单页驱逐（2026-11-07）

`debugLogEmu8.txt` 证明每帧交替的 512×240 写入来自真实 GP0 Block Fill，
而不是普通 Tile 软件镜像。被失效的 4358 个条目中有 3739 次同帧匹配 miss，
因此不能跳过 Block Fill 的 VRAM 写入或缓存失效。

真正的上传尖峰来自 41 次 `130×130` 分配失败：64 张 atlas 虽仍有大量空闲
矩形，但最大空闲矩形只有 `255×125`。旧代码会清空全部 atlas 元数据，使仅
占 6.9% 的帧产生 48.9% 的 miss。

Phase 1B 保持 8 个 GX TMEM region 和上传同步逻辑不变，只替换 atlas 的 CPU
元数据驱逐策略：

1. 分配失败时统计每张 256×256 atlas 仍被有效条目引用的次数；
2. 选择有效条目最少的 atlas，同代价时轮转，并尽量避开最近使用的 atlas；
3. 仅清除指向该 atlas 的条目并重置它的矩形分配表；
4. 新纹理仍通过原有 OpenGX 上传路径写入，继续使用该路径的 GPU 同步；
5. 其余 63 张 atlas、纹理对象和 8 个 TMEM region 均不清空。

新日志中 `atlaslocal=次数/删除条目` 与 `localevict=次数/删除条目` 记录单页
驱逐；`atlasclear` 只保留为异常防御计数。`GXALLOC pressurepre` 是驱逐前的
atlas 几何快照。下一轮应重点确认 `atlasclear=0`、单次删除条目远低于全局
元数据数量，以及清空帧原有的 177～346 次 miss 尖峰消失。

`debugLogEmu9.txt` 中全局清空已降为 0，miss/帧由 37.4 降至 17.9，但敌人
出现时仍会因 `130×130` 请求在一帧内局部驱逐 30～45 张 atlas。每次仅删除
一个条目，说明保护边令一个 128×128 大纹理独占一张 256×256 atlas，64 张
不足以容纳该场景的大纹理工作集。为验证容量而不改变发布默认值，新增可选
编译参数：

```sh
make GX_TEXPATH_PROFILE=1 GX_TEXPATH_PHASE1A=1 \
     GX_TEXPATH_ATLAS_COUNT=96 GPU=GX Wii
```

该阶段保留 64 为默认值并用 96 验证容量收益。后续 Wii3 实机测试确认 96 张
atlas 全部使用，且 Phase 1C 可跳过 86.15% 的 subimage 上传等待；但纹理堆内
只剩 499 KiB（另有不计入该日志的 1 MiB 系统硬保留）。曾继续测试 88 张以
换取约 1 MiB 空间，但特定游戏在 MEM1 接近耗尽时暴露出 JIT 分配风险。最终
决定恢复 64 张作为发布默认值，不加入 JIT 从 MEM1 转到 MEM2 的后备分配逻辑；
仍可通过 `GX_TEXPATH_ATLAS_COUNT` 做专项测试覆盖。

#### Phase 1C：非驻留 subimage 上传跳过全局等待（2026-11-07）

`debugLogEmu10.txt` 中 96 张 atlas 将解码和上传量降低约 43%，但每次
`glTexSubImage2D()` 仍无条件执行一次 `GX_DrawDone()`。Phase 1C 先采用最小
安全范围，不改变 resident backing 的同步方式：

1. 更新前扫描 `texCacheUsedInfo[8]`；
2. 同时检查当前 texture ID 及其半透明 companion
   `ID + _MAX_GL_TEX`；
3. 两者均不在 8 个 region 中时，直接修改 MEM2 backing 并 flush，跳过
   `GX_DrawDone()` 与全局 `resetTexCacheInfo()`；
4. 任一对象仍驻留时，完整保留旧的 `GX_DrawDone()` 和全局 reset；
5. `glTexImage2D()`、movie、delete、readback 均不改动。

该路径要求 Phase 1A 的跨帧 region 登记，编译时使用：

```sh
make GX_TEXPATH_PROFILE=1 GX_TEXPATH_PHASE1A=1 \
     GX_TEXPATH_PHASE1C=1 GPU=GX Wii
```

`GXTEX uploadsync=skip/residentWait` 记录跳过和保守等待次数；`drawdone` 现在
只统计真正执行的上传等待。正常情况下 `skip + residentWait == subimage`。
若画面正确且 skip 占比高，再考虑利用已经完成的 region fence 进一步减少
residentWait；第一版不改变 resident 情况，便于隔离正确性回归。

#### Phase 1A.2：复用已经完成的栅栏

初版在每次缓存满时先给全部 pending region 创建 DrawSync token，再寻找可
替换槽，因此实测 `fence` 基本等于 `evict`。Phase 1A.2 调整为：

1. 先读取当前已完成 token；
2. 在“不属于 pending 且旧 token 已完成”的 region 中选择 LRU；
3. 只有没有候选时，才给当前 pending 集合创建一个新 token；
4. 新 token 尚未完成且仍无安全候选时，继续回退 `GX_DrawDone()`。

pending 检查是必要条件：region 在旧 token 后再次被 draw 使用时，即使旧
token 已完成也不能据此淘汰。该调整不改变安全边界，目标是让一个已经完成的
栅栏支持后续多次单槽替换，使 `fence` 明显低于 `evict`。

`debugLogEmu3.txt` 表明上述提交时机仍然过晚：虽然 `fence/evict` 降至
约 22.9%，但 `wait/fence` 为 100%。没有安全候选时才提交 token，随后立即
查询，GPU 不可能获得足够的重叠执行时间；日志中还出现几乎没有上传却有
37～38 次淘汰等待的帧。因此这个版本只适合诊断，不作为最终调度方案。

#### Phase 1A.3：低水位异步栅栏

Phase 1A.3 保持同一个编译开关，但将栅栏改为提前调度：

1. 同一时刻最多保留一个 outstanding DrawSync token；
2. 初次填充时，空闲 region 降到 3 个便为当前 pending 集合提交 token；
3. 淘汰时先更新 outstanding token 的完成状态并选择已经安全的 LRU；
4. 已安全候选降到 3 个且没有 outstanding token 时，提前为下一批 pending
   region 提交 token；
5. CPU 继续消耗剩余安全候选，让 GPU 在此期间处理 token；
6. 只有安全候选确实耗尽而 token 仍未完成时，才回退 `GX_DrawDone()`；
7. 已经耗尽时不再“提交 token 后立即查询”，因为这种操作没有异步收益。

实现时不能从 `OGX_DRAW_SUBMITTED()` 直接发出 GX 命令：部分专用绘制函数在
`GX_Begin()` 与 `GX_End()` 之间调用这个标记。若此时执行 `GX_SetDrawSync()`
或 `GX_Flush()` 会把控制命令插入尚未结束的 primitive，破坏 GX FIFO，并在
模拟器中直接触发运行错误。因此 draw 回调只更新 pending 和 fence-request
标记；真正的 token 在下一次 `_ogx_apply_state()`（位于 `GX_Begin()` 之前）
提交。即使标记发生在 primitive 顶点写入之前，token 也会在下一次 draw 的
安全边界写入 FIFO，顺序仍位于上一 primitive 的 `GX_End()` 之后。

低水位 3 是首轮测试值。下一轮主要比较 `evict=single/fence/wait` 中的
`wait/fence` 和 `wait/single`；若等待仍集中出现，可根据真机数据调整为 4，
若 fence 过多而等待已经很低，则可尝试降为 2。

### Phase 2：Indirect window 最近邻原型

范围：

- 仅支持 16、32、64、128、256；
- 仅支持 nearest filtering；
- 8 texel、linear filtering 和异常参数继续走 `LoadTextureWnd()`；
- 先保留 RGB5A3 CPU 解码，不同时引入 TLUT。

目标是验证坐标数学、TEV 状态和真实性能，不追求一次覆盖全部游戏。

### Phase 3：Window atlas 与 wrap gutter

- 将 window 内容放入主 atlas；
- 增加环绕保护边；
- 支持 linear filtering；
- 验证纹理边缘、mirror 和 sprite UV 修正；
- 旧独立窗口缓存仅用于回退。

### Phase 4：CI4/CI8 + TLUT

建议顺序：

1. 非半透明 4-bit 纹理；
2. 非半透明 8-bit 纹理；
3. 透明色索引 0；
4. STP 分类和双 pass；
5. CI texture 与 indirect window 组合；
6. TLUT 缓存和替换策略。

15-bit 纹理继续使用 RGB5A3。

### Phase 5：默认启用与旧缓存退役

只有在真机回归和性能数据均通过后，才考虑默认启用。旧 `wcWndtexStore` 至少保留一个发布周期作为兼容后备。

## 6. 验收标准

### 6.1 正确性

重点验证：

- RRT4：高频 texture window；
- Vagrant Story：加载页和复杂纹理状态；
- Metal Gear Solid：framebuffer/特殊纹理反馈；
- Dino Crisis 1/2：纹理和 VRAM 同步问题；
- Final Fantasy VII/IX：菜单和纹理页切换；
- 4-bit、8-bit、15-bit 各至少一个测试场景；
- 四种 PS1 ABR 半透明模式；
- nearest、linear 两种过滤；
- 8/16/32/64/128/256 texture-window 尺寸。

画面必须与 legacy path 对比，重点检查：

- 窗口边缘串色；
- 透明黑色；
- STP 纹素错误混合；
- CLUT 改写后复用陈旧纹理；
- VRAM 写入后缓存未失效；
- sprite mirror 和 UV 偏移。

### 6.2 性能

至少记录：

- 每帧 texture-window miss；
- 每帧上传字节数；
- 每帧 `GX_DrawDone()` 次数；
- CPU 纹理解码时间；
- 平均帧时间和 99th-percentile 帧时间；
- 音频欠载或帧跳过次数。

成功标准不是只提高平均 FPS，而是减少 texture-window 重度场景中的长帧和流水线停顿，同时不显著降低普通场景性能。

## 7. 主要风险

| 风险 | 后果 | 缓解方式 |
|---|---|---|
| indirect 坐标精度不足 | UV 边界抖动或错位 | 最近邻原型、真机截图、8 texel 回退 |
| bilinear 跨 atlas 块采样 | 边缘串色 | wrap gutter、半 texel 校正 |
| TMEM/map 槽冲突 | 纹理错误或频繁换入 | 预留槽位、重构 cache-region 管理 |
| TLUT 抖动 | CI 路径反而变慢 | TLUT 命中统计、批次化、有限 LRU |
| STP 语义丢失 | 半透明画面错误 | 保留双 pass，按 palette alpha 分类 |
| 延迟同步覆盖 in-flight 数据 | 随机纹理损坏 | token/generation、双缓冲、旧路径开关 |
| 同时改动过多 | 难以定位回归 | 严格按 Phase 分支和提交 |

## 8. 推荐结论

GX 可以实现类 OGL2 的纹理架构，但应采用 GX 专用方案，而不是尝试模拟通用 shader：

- texture window：优先使用 indirect texture wrap，使窗口进入主 atlas；
- CLUT：逐步验证 CI4/CI8 + TLUT；
- 同步：优先减少纹理上传中的无条件 `GX_DrawDone()`；
- 兼容性：始终保留 legacy RGB5A3 和独立 window cache 回退路径。

预期收益最大的场景是 texture-window 高频、CLUT 切换频繁且当前存在大量纹理上传同步的游戏。普通游戏的收益可能较小，因此必须以计数器和真机基准决定是否默认启用。
