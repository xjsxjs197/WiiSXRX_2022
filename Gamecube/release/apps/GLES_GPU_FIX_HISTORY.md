# WiiStation GLES GPU 修正履历

本文档记录 WiiStation/WiiSXRX 的 GLES GPU 兼容性修正，范围从《恐龙危机2》水下人物不显示问题开始。内容只保留问题原因和最终处理方式，方便以后继续追加。

## 2026-08-18：恐龙危机2水下人物不显示

提交：`f0fb930 Add GX VRAM readback synchronization`（经 `5220850` 合并）

- 原因：人物已经由 GX 绘制到 EFB，但游戏随后读取的 PS1 VRAM 软件副本仍是旧数据。
- 修正：增加 GX EFB 到 PS1 VRAM 的回读状态机，并在已确认的 VRAM 读取和 MoveImage 路径同步数据。

## 2026-08-19：回读颜色通道错误

提交：`18c0b27 GlesGpu: fix RGB5A3 readback channel order`

- 原因：GX RGB5A3 与 PS1 15-bit 像素的红、蓝通道位序不同。
- 修正：回读转换时正确交换颜色通道。

## 2026-08-19～08-30：纹理缓存失效 T6 方案（已回滚）

提交：`dfe56bc Merge TextureCacheInvalidation T6 texture freshness`、`04fde1d Revert "Merge TextureCacheInvalidation T6 texture freshness"`

- 目标：对 CPU、GPU、MoveImage 等 VRAM 写入建立统一的纹理依赖和失效处理。
- 结果：方案范围较大，增加了纹理解码和回读开销，并存在兼容性风险，因此整体回滚。
- 结论：后续优先采用游戏特征明确、影响范围小的修正。

## 2026-09-01：VRAM 回读和画面提交异步化

提交：`73f6d89 GlesGpu: make VRAM readback and present asynchronous`

- 原因：每次复制后调用 `GX_DrawDone` 会阻塞 CPU/GPU，造成帧率下降和声音卡顿。
- 修正：使用 GX callback 管理异步复制；未完成时继续使用上一份有效画面。

## 2026-09-03：只在真正读取时等待异步回读

提交：`efe8f2c GlesGpu: wait for async snapshots at VRAM read barriers`

- 原因：异步复制尚未完成时，如果游戏立即读取 VRAM，可能得到旧帧，导致人物偶尔消失。
- 修正：普通帧保持异步；只有 C0 读取或 MoveImage 马上需要数据时才等待并发布快照。

## 2026-09-06：增强 CLUT Key 和半透明纹理刷新

提交：`a491ea7 GlesGpu: strengthen CLUT keys and refresh semitrans textures`

- 原因：旧的调色板 checksum 可能碰撞，不同 4-bit/8-bit CLUT 会错误复用同一缓存；半透明纹理数据也可能没有及时刷新。
- 修正：使用包含 CLUT 地址、模式、半透明属性和更强哈希的 64-bit Key，并同步更新半透明纹理数据。

## 2026-09-06：合并并收敛 VRAM 回读逻辑

提交：`b004f87 Merge VramReadback updates`

- 原因：全局回读会影响大量本来正常的游戏和场景。
- 修正：把回读限制到已确认的游戏和命令特征，并保留异步复制与读取屏障。

## 2026-09-08～09-10：魔屋鬼影4残留门纹理

提交：`4fd6a87 GlesGpu: fix AITD4 framebuffer texture feedback`、`1fecb9d GlesGpu: refine AITD4 framebuffer feedback`

- 原因：游戏把 EFB 中刚生成的画面再次作为纹理使用，但 CPU VRAM 中仍是旧的门或 UI 数据。
- 修正：只对该游戏已确认的全屏反馈纹理直接捕获 EFB；跳过全零来源并修正采样坐标，避免黑屏和人物模糊。

## 2026-09-12：降低恐龙危机2回读开销

提交：`bb4296b GlesGpu: halve Dino Crisis 2 readback snapshots`

- 原因：640×480 全尺寸 EFB 快照的内存写入和 CPU 转换开销较大。
- 修正：在确认的 320×240 游戏映射下使用 2:1 GX Copy，生成 320×240 快照，数据量降为四分之一。

## 2026-09-13：寄生前夜2红色警告背景残留

提交：`f46e65f Add Parasite Eve II EFB clear workaround`

- 原因：游戏交替使用两个 PS1 显示页，但 GX 只有一个 EFB；保留 EFB 会让上一页的半透明红色效果成为下一页的混合背景。
- 修正：仅对《寄生前夜2》恢复每次 present 后清空 EFB。

## 2026-09-14：合金装备存档说明文字缺失

提交：`603d4e8 GlesGpu: restore MGS save description feedback`

- 原因：白色说明文字先生成在 EFB 临时区域，随后立即作为纹理读取；CPU VRAM 对应区域仍是空白。
- 修正：识别该操作后把 EFB 捕获为 I8 强度纹理，并在立即使用前完成复制。

## 2026-09-15：恐龙危机1扭曲球体显示为黑块

提交：`94ebeba GlesGpu: fix Dino Crisis framebuffer feedback`

- 原因：扭曲效果通过多个 64×64 MoveImage 区块读取刚绘制到 EFB 的内容，CPU VRAM 中没有这些新像素。
- 修正：只同步所需的 64×64 EFB 区域，进行 2:1 RGB5A3 转换后写回 PS1 VRAM，避免整帧回读。

## 2026-09-21：FF7、FF9 战斗菜单不显示

提交：`5dccaf5 Fix FF7 and FF9 menu display pages`

- 原因：游戏在当前显示页和上一显示页之间绘制菜单，GLES 固定使用上一页作为坐标基准，导致菜单图元被换算到屏幕外。FF9 主要左右换页，FF7 主要上下换页。
- 修正：根据 Drawing Area 实际命中的显示页动态选择坐标基准，并通过游戏 ID 只对 FF7、FF9 启用。
- 验证：FF7、FF9 已在 Wii 实体机验证，战斗菜单显示正常。

## 后续追加格式

```text
## YYYY-MM-DD：问题或游戏名称

提交：`提交号 提交说明`

- 原因：
- 修正：
- 验证：
```
