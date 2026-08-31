# 毛玻璃效果交接笔记

本文件是毛玻璃功能的维护入口。它记录了调研来源、当前架构、踩过的坑和后续工作；不是视觉规范，也不替代代码注释。

## 范围与来源

实现目标是让 Noctalia 的 bar、dock、面板和具有**宿主标准背景**的桌面小部件，从同一输出上的壁纸中取样，渲染一致的毛玻璃材质。

没有复制或移植第三方代码。首轮调研（`GLASS_EFFECT_RESEARCH.md`，2026-08-30）实际参考了下列项目；链接和可借鉴范围在此保留，避免后续重新搜寻。

| 项目 | 有用的参考点 | 对本实现的结论 |
| --- | --- | --- |
| [OverShifted/LiquidGlass](https://github.com/OverShifted/LiquidGlass) | C++/OpenGL 的 framebuffer、独立 blur pass、squircle SDF 折射，以及 blur/noise/glow 参数组织 | 首选的管线和参数设计参考。其依赖自有 OverEngine 与 ImGui，不能整体移植；仅吸收 shader 数学与资源组织思想。调研时项目页标注 MIT License。 |
| [zaroutt/Niri-glass](https://github.com/zaroutt/Niri-glass) | `clipped_surface.frag` 的 Niri 液态玻璃视觉参数：refraction strength/power、edge lighting、vibrancy、lens distortion、fringing | 用作视觉和 shader 参数参考。它直接修改 compositor 并采样 compositor 背景，架构不适用于仅使用壁纸的 Noctalia；若复制任何代码，须先单独确认许可证。 |
| [ybouane/liquidglass](https://github.com/ybouane/liquidglass) | WebGL 的背景裁剪、Gaussian blur、refraction、色散、Fresnel、specular、inner rim、shadow 管线 | 用作材质拆分和可调参数参考。其 DOM snapshot 输入不适用于 Wayland；Noctalia 将输入替换为输出级壁纸纹理。调研时项目页标注 MIT License。 |
| [Hla-aung/liquid-glass](https://github.com/Hla-aung/liquid-glass) | Snell 定律折射、displacement map、Fresnel rim 和多种 lens profile，尤其是 `ConvexSquircle` | 用作光学公式/边缘模型参考，不作为运行时依赖。调研时项目页标注 MIT License。 |

### 后续延伸阅读（非本次实现参考）

下列项目是在编写本交接文档时补充检索的运行时资料；它们**没有**参与本次玻璃架构、shader 或参数的设计和编码。Wayfire 是实际测试合成器，相关日志仅用于兼容性验证。

| 项目 | 有用的参考点 | 对本实现的结论 |
| --- | --- | --- |
| [KWin Blur effect](https://github.com/KDE/kwin/tree/master/src/effects/blur) | 离屏 blur 与区域/损伤处理 | 后续若优化 blur 缓存可阅读；未用于本次实现。 |
| [Wayfire](https://github.com/WayfireWM/wayfire) 的 [blur 配置说明](https://github.com/WayfireWM/wayfire/wiki/Configuration#blur) | compositor blur 的开关及性能约束 | 实际测试环境；用于确认不应依赖 compositor blur 作为壁纸采样源，并未借鉴其实现。 |
| [SceneFX](https://github.com/wlrfx/scenefx) | wlroots 场景图效果的后续参考 | 后续扩展圆角/裁剪/blur 时可阅读；未用于本次实现。 |

macOS Liquid Glass 是视觉方向参考（透明、色调提高可读性、边缘光学效果），不是可复用的开源实现。

## 当前架构

```text
壁纸 Surface
  └─ BackdropSurface：输出级 sharp + blurred FBO
       └─ GlassBackgroundRegistry（以 wl_output 名称发布快照）
            └─ GlassNode
                 └─ GlassProgram：壁纸取样 + 边缘折射/色散 + tint/border
                      ├─ Bar / Dock
                      ├─ 浮动和附着 Panel
                      └─ DesktopWidgetsHost 的标准背景
```

关键入口：

- `src/shell/backdrop/backdrop_surface.cpp`：生成并发布每个输出的 sharp/blur 纹理；玻璃必须有此来源。
- `src/render/glass/glass_background_registry.*`：跨 surface 的输出级纹理注册表。
- `src/render/scene/glass_node.h`、`src/render/programs/glass_program.cpp`：形状、材质和 shader。
- `src/shell/surface/output_local_rect.h`：统一 layer-shell 到输出局部坐标的解析。新增玻璃消费者时优先使用它，不要自行猜 margin/anchor。
- `src/shell/desktop/desktop_widgets_host.cpp`：标准桌面小部件背景玻璃化；插件内容只是叠加在宿主玻璃层上。

## 配置与材质

`[shell.panel]` 下的相关项：

- `transparency_mode = "glass"`
- `glass_preset = "subtle" | "clear" | "tinted" | "frosted" | "bold"`
- `glass_opacity`：整块玻璃最终 alpha。
- `glass_blur_intensity`：映射为 0–20 logical px 的壁纸模糊半径。
- `glass_refraction_strength`：边缘折射和 RGB 色散的线性倍率。

选择 `glass` 时，设置层会原子性地启用 `backdrop.enabled`，并关闭 dock 与所有现有 bar 的普通阴影，避免阴影与玻璃边缘重复。

预设参数定义在 `src/render/glass/glass_material.h`。`tinted` 是提高可读性的白色色调模式：高白色 tint、较弱折射，仍保留少量壁纸层次和边缘高光。

## 坐标规则（最容易出错）

玻璃纹理的 UV 必须对应**物理上位于同一输出位置的壁纸**，而不是对应 surface 的 `(0, 0)`。

1. 普通 layer surface：使用 `OutputLocalRect` 解析 anchor 和 margin 后的输出局部 origin。
2. bar 的附着面板：exclusive zone 会改变 surface placement；保留打开时记录的可视矩形，不能仅依据 margin 推导。
3. dock：水平 dock 只锚定底部，X 是居中计算的；不能假设 `marginLeft == 0`。
4. 桌面小部件：surface 是旋转后 AABB，内部内容还有 rotation/flip。`GlassNode::setOutputTransform()` 接收完整仿射矩阵，将玻璃局部坐标映射回输出坐标；只传平移会导致旋转或镜像时取样错位。

`GlassProgram` 中 `u_output_transform` 是上述规则的最终接口。对未变换节点，`GlassNode::setOutput()` 会建立纯平移矩阵，现有 bar/dock/panel 调用无需特殊处理。

## 已验证的经验与常见回归

- 合成器背景 blur 是补充，不是玻璃源；若只有纯白/纯色层，先检查 backdrop 是否发布了 sharp/blur 纹理。
- 不要把普通 `Box` 背景和 `GlassNode` 同时设为不透明。玻璃模式下，标准背景 box 只保留布局、圆角和裁剪，填充 alpha 应为零。
- 玻璃噪点要非常低。把 tint alpha 当作噪点强度会产生明显颗粒感。
- 折射在高 blur 下会被掩盖；排查时先将 blur 降低，再增大 refraction，或使用显眼的测试壁纸。
- blur 不能在某个阈值突然启用，也不能将半径量化为整数 tap；当前 shader 为分数 tap 做权重衰减，半径调节应连续。
- GLSL 的未使用 uniform 可能被驱动优化掉。`ensureInitialized()` 只能要求实际在 shader 中使用的 uniform，否则会出现 `failed to query ... shader locations`。
- 第三方插件自行画出的任意 `ui.box` 无法由宿主可靠识别。当前自动玻璃化只覆盖 `DesktopWidget::setBackgroundStyle()` 创建的标准背景；若需自定义背景玻璃，应设计显式插件 API/manifest opt-in。

## 调试清单

1. 用包含彩色网格、坐标文字和锐利边缘的壁纸验证位置、Y 翻转和折射。
2. 确认日志包含：

   ```text
   [backdrop] wallpaper backdrop support: ... supported=true
   [backdrop] creating on ...
   [glass-background] published output ...
   ```

3. 按顺序检查：输出名称是否匹配 → backdrop 是否发布 → `GlassNode` 是否可见 → 普通背景是否透明 → output transform 是否正确。
4. 分别验证：顶/底 bar、水平/垂直 dock、浮动 panel、附着 panel、桌面小部件、旋转/镜像桌面小部件、多输出。
5. 只改材质滑块时应触发重绘而非销毁重建 bar/dock/插件小部件。

## 后续候选工作

- 为桌面插件提供显式的 `glass` 背景声明/API。
- 为深色主题添加可读性优先的深色色调预设，而不是强行复用白色 `tinted`。
- 为坐标变换和分数 blur 权重补充 GPU/渲染回归测试。
- 评估 backdrop FBO 的更新/缓存策略，尤其是动态壁纸和高分辨率多输出场景。
