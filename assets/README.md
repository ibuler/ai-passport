<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

Pocket Focus uses [`fonts/SourceHanSansSC-Regular.otf`](fonts/SourceHanSansSC-Regular.otf)
from [Adobe Source Han Sans](https://github.com/adobe-fonts/source-han-sans/tree/release/OTF/SimplifiedChinese),
under the SIL Open Font License in [`fonts/LICENSE.txt`](fonts/LICENSE.txt).
[`fonts/pocket_font_16.c`](fonts/pocket_font_16.c) is generated with `lv_font_conv` 1.5.3,
16 px, 2 bpp, no compression or kerning, for ASCII, U+3000–301F, U+4E00–9FFF,
and U+FF01–FF60. It is compiled into the application. The exact conversion command
is retained in its header; actual coverage and rendering are tested by `tests/pocket`.
The [`storage/.keep`](storage/.keep) file retains the empty source directory for the
preformatted application SPIFFS image. See the [application guide](../docs/pocket-focus.md).


- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

The XiaoZhi app uses [`images/furina/furina-sheet-soft.png`](images/furina/furina-sheet-soft.png), an alpha-background and soft lower-silhouette edit of [`furina-sheet.png`](images/furina/furina-sheet.png), a 1254×1254, 3×3 expression sheet generated with the built-in image tool on 2026-10-22 for the user's Furina selection. This is generated fan art; the Genshin Impact character design belongs to HoYoverse, and no blanket redistribution license is asserted. The final prompt, frame order, conversion and hashes are in [`images/furina/metadata.json`](images/furina/metadata.json). [`images/furina/furina.rgb565a8`](images/furina/furina.rgb565a8) stores nine 128×128 RGB565A8 frames (442,368 bytes; color plane followed by alpha plane per frame), embedded in application Flash and referenced directly by LVGL without allocating full frames. `tools/pocket/embed_portrait.py` supplies the same bytes to the native renderer. These are application assets and do not replace the user's uploaded home image.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

User-provided home-screen layout reference: [`images/reference/passport-home-reference.png`](images/reference/passport-home-reference.png), 900×1200 PNG, supplied on 2026-10-22. Retained only for local design reference; not embedded in firmware. Redistribution rights were not supplied; do not publish this photograph without permission.
