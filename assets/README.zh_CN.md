<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

小智现使用《完美世界》成年火灵儿：[`images/huolinger/huolinger-sheet.png`](images/huolinger/huolinger-sheet.png) 是 2026-10-23 通过内置图像工具生成的透明 3×3 表情图集。[`images/huolinger/huolinger.i4`](images/huolinger/huolinger.i4) 保存九帧 128×128 I4，每帧含 16 项 BGRA 调色板，**总计 74,304 字节**，比原来的 442,368 字节减少 83.2%。所有表情共用十五种不透明颜色及一个透明项，避免换帧时颜色跳变。[`images/huolinger/huolinger.png`](images/huolinger/huolinger.png) 预览实际打包像素。成年形象采用红金服饰、棕红长发和凤凰发饰，属于生成的同人画，并非官方素材；原角色权利归各权利方，不声明统一再分发许可。

最终提示词、帧顺序、转换命令、视觉参考及哈希记录在 [`images/huolinger/metadata.json`](images/huolinger/metadata.json)。`tools/pocket/convert_portrait.py` 需要 Pillow，可从源图重现显示资源；`tools/pocket/embed_portrait.py` 为原生测试提供相同字节。LVGL 从 Flash 读取调色板及索引，每次只展开一行 512 字节 ARGB 像素，另有解码器管理及分配器开销，不使用完整帧缓冲或解压器。保持 `LV_BIN_DECODER_RAM_LOAD` 关闭；`main/pocket/portrait.c` 会拒绝不兼容配置。原芙宁娜素材及其 [`metadata.json`](images/furina/metadata.json) 保留作历史参考，不再嵌入固件。这些应用头像不替换用户上传的首页图片。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

## Pocket Focus 应用素材

[`fonts/SourceHanSansSC-Regular.otf`](fonts/SourceHanSansSC-Regular.otf)来自 [Adobe 思源黑体](https://github.com/adobe-fonts/source-han-sans/tree/release/OTF/SimplifiedChinese)，许可证为 [`fonts/LICENSE.txt`](fonts/LICENSE.txt) 中的 SIL Open Font License。生成的 [`fonts/pocket_font_16.c`](fonts/pocket_font_16.c) 使用 `lv_font_conv` 1.5.3，16px、2bpp、不压缩、不含 kerning，范围为 ASCII、U+3000–301F、U+4E00–9FFF、U+FF01–FF60，直接编译进应用。转换命令保留在生成文件头部，`tests/pocket` 检查实际字形覆盖和渲染。[`storage/.keep`](storage/.keep) 保留用于生成预格式化 SPIFFS 镜像的空源目录。详见[应用指南](../docs/pocket-focus.zh_CN.md)。

用户于 2026-10-22 提供的首页布局参考：[`images/reference/passport-home-reference.png`](images/reference/passport-home-reference.png)，900×1200 PNG，仅留作本地设计参考，不嵌入固件。未提供再分发授权，未经许可不要发布此照片。
