[简体中文](pocket-focus.zh_CN.md) · **English**

# Pocket Passport

Pocket Passport is a personal card and small-app launcher built on the official AI Passport BSP. The upper home area puts the uploaded card image, name, and company on the left, with clock/date/city and an illustrated weather condition on the right. The lower area displays only a contribution heatmap, without a GitHub title or week label, above a single button-hint line. The card has no separate background fill. The seven apps are XiaoZhi with adult Huo Ling'er, music, city radio, weather, a focus timer, a stopwatch, and phone settings. This is an independent UI, not the baseline hardware-test menu or a copy of community play 438. Development continues on `feature/pocket-focus`, based on Gitee `main` at `a06235e4c94361e3f8a2d498930254101718acae`.

## Use

| Screen | Control | Result |
| --- | --- | --- |
| Home | Hold OK | Open the app list |
| App list | UP / DOWN, then OK | Select and open an app |
| App list | Hold OK | Return to the personal card |
| XiaoZhi | OK | Start continuous conversation; automatic turn detection/reply; press again to close |
| XiaoZhi | Hold UP / DOWN / OK | Increase / decrease shared volume / close the voice session and return to apps |
| Music / Radio | OK | Start, pause, resume, or retry; entering does not autoplay |
| Music / Radio | UP / DOWN | Switch the configured track or current-city station and play |
| Music | Natural track completion | Play the next track in order; wrap the last track to the first (a one-track list repeats) |
| Music / Radio | Hold UP / DOWN | Increase / decrease volume by 5 points per hold, clamped to 0–100%; show and save the shared music/radio volume |
| Weather | OK | Request an update |
| Focus timer | OK | Start/pause/resume; start a new round after completion |
| Focus timer | UP / DOWN while paused | Set 5–60 minutes in five-minute steps and reset the round |
| Stopwatch | OK / UP | Start or pause / reset |
| Any app | Hold OK | Stop audio or close settings and return to the app list |
| Home, app list and other apps | Hold DOWN | Open phone settings |

The battery value remains in the top-right corner and degrades to `--%` when unavailable. Clock time requires network synchronization; the configured fixed UTC offset defaults to UTC+8 and does not automatically follow daylight saving time. Focus and stopwatch use monotonic time, continue across page navigation, and reset at reboot. Focus completion is visual. Deep sleep and OTA are not implemented.

### Phone settings

From music/radio, hold OK to return to the app list first. Hold DOWN, connect to the SSID/password shown on the device, keep that connection even if it reports no Internet, and open `http://192.168.4.1`. Verify and save a 2.4 GHz Wi-Fi network first. A failed connection restores the saved network; a channel change may require reconnecting the phone.

- **Name and music:** edit the name, volume, and up to eight named MP3 direct URLs, or import a version-1 playlist JSON by URL/local file. The URL field defaults to the supplied OSS Mayday playlist. Import replaces the draft list in order only after all tracks validate; errors preserve the draft and stored configuration. Review/edit, then use the name/music save button. Playlists must contain 1–8 tracks and fit 7 KiB; extra metadata is ignored and signed audio URLs are preserved exactly. The device fetches direct JSON URLs over HTTP/verified HTTPS (no redirects), so phone CORS is not required; device Wi-Fi/time must be ready. Local file import works offline. Saved tracks are a snapshot, not an automatically refreshed subscription.
- **Company, city, and radio:** edit the company, choose a preset city (including Tianjin) or supply a city name, set the UTC offset, and optionally add up to six manual city/station/MP3 URL entries. Use the company/city/radio save button. Coordinate inputs are removed. Presets resolve locally; existing custom-city coordinates remain compatible, while a new custom city is resolved by name through Open-Meteo when weather is requested. A same-name search uses its first result; provide an unambiguous city name. Weather and radio use this same configured city, without computer/GPS location access.
- **GitHub:** the separate GitHub ID field/save button defaults to `ibuler`; leave it empty to stop fetching and display the default graphic. IDs allow up to 39 ASCII letters/digits with single internal hyphens. The ID is stored independently in the `github_id` NVS string, preserving existing profile/config/image data.
- **Card image:** upload PNG, JPEG, WebP, or GIF separately. The color and animation checkboxes independently control color and animation. Conversion is local in the browser using bundled gifuct-js 2.1.2. Output is center-cropped 96×96 RGB565, at most 48 frames with 100–2000 ms delays; the original file is limited to 20 KiB (20,480 bytes), checked before reading or decoding. GIF input is still limited to 1024×1024 and 300 source frames. Long GIFs are sampled. The converted RGB565 upload may exceed 20 KiB; its independent device format/length checks remain in place. The source-file cap does not reduce the fixed 18,432-byte playback frame buffer.

Hold OK after saving to close the AP/HTTP service and return to the app list. Hold OK again to return home. Name/company/track fields allow 95 UTF-8 bytes in firmware; city fields allow 63. The phone editor uses conservative character limits. Unsupported emoji or extended Han glyphs become `?`. The actual CJK font is tested, not inferred from a successful build.

### GitHub contributions and weather icons

The heatmap reads GitHub's public [contribution calendar](https://github.com/users/ibuler/contributions) directly over verified HTTPS, without a token or third-party proxy. It shows the most recent 26 Sunday-first weeks with the published levels 0–4; future dates remain blank. It does not infer activity from repository commit counts. A bounded streaming parser consumes at most 384 KiB of HTML, retaining only a 512-byte tag and the 182 daily levels, with a 15-second body deadline and cancellation for settings/audio. Login/error/truncated/incomplete calendars fail validation. The public page format can change; it retains the previous same-user RAM snapshot when available. Without valid data (first boot, offline, missing account or blank ID), it displays the built-in default graphic without home error/title text. The default is an ibuler public-calendar snapshot captured on 2026-10-22, used as sample decoration, not a claim of live activity for the configured account; phone settings explicitly explain this. Switching IDs clears the previous user's live graph. Reboot clears this RAM cache.

Fetching starts after network/time synchronization and pauses while settings or audio is active, sharing the existing HTTP allocation mutex. Successful data refreshes hourly; without valid data failures retry after five minutes. Closing phone settings requests another refresh. An empty weather city does not disable GitHub. The I4 image uses 5,784 bytes and one LVGL object only while home is visible; leaving home releases its pixels. This avoids one widget per day. Weather icons are application-drawn sun/cloud/rain/snow/fog/lightning shapes selected from the [Open-Meteo weather codes](https://open-meteo.com/en/docs); unknown/missing data gets a neutral dash. The icons do not require emoji fonts.

### Weather and city radio

Weather uses [Open-Meteo](https://open-meteo.com/en/docs), requests current temperature, weather code, humidity, and wind, and retries on OK or at fifteen-minute intervals when audio is idle. The weather app identifies offline/failed/stale data; there is no invented weather or fallback to a different city. Attribution is displayed in the weather app. Data is a model estimate at the preset, retained, or [name-resolved location](https://open-meteo.com/en/docs/geocoding-api). Location lookup failure reports unavailable weather rather than querying the previous city.

The radio is **Internet radio**, using the [Radio Browser API](https://docs.radio-browser.info/) with the configured city name, MP3 filter, and at most six results. It searches station names, not physical FM frequencies or guaranteed geographic coverage. Manual entries whose city exactly matches the current city take priority. No-result or directory-failure messages remain visible; users can retry or add a known direct stream. AAC, HLS, M3U, login, DRM, and web sharing pages are not supported by this MP3-only player. Switching cities cannot reuse the previous city's station list.

Music streams HTTP/HTTPS MP3, including chunked responses and up to four redirects. The default example remains episode 79 of [Music For Programming](https://musicforprogramming.net/), streamed from its publisher rather than redistributed. TLS uses the IDF certificate bundle and SNTP; validation is not disabled. Long pauses may require a restart from the beginning; seeking and saved progress are absent. After starting music with OK, successful natural completion advances in playlist order and wraps to the first track. Errors remain on the current track for manual retry; paused playback and leaving the music page do not advance. The title/selection follows automatic changes, and radio does not use playlist looping. After the MP3 memory repair, the user confirmed audible playback from the OSS source on 2026-10-22. The new volume controls still require separate device acceptance.

## Implementation

### XiaoZhi and adult Huo Ling'er

The app list includes XiaoZhi with an adult Huo Ling'er portrait from Perfect World. It uses the [FoloToy reference](https://github.com/FoloToy/folo-ai-passport-xiaozhi/tree/72da544d1b51678f5d88967adc299b3aec956946) communication protocols on the existing ESP-IDF 5.5.3 BSP; it does not replace this application's firmware, UI, partitions or provisioning. The upstream distribution requires IDF 6.0.1+, so its whole application is not embedded here. Initial OK obtains activation/connection configuration from the official XiaoZhi endpoint. If an activation code appears, add the device at [xiaozhi.me](https://xiaozhi.me) within the three-minute activation window; otherwise retry OK. Voice/personality is selected in that service's console; the portrait alone does not change the voice or impersonate a voice actor.

Continuous conversation uploads mono 16 kHz, 60 ms Opus microphone frames with server-side automatic turn detection. Press OK once to open; recognition automatically pauses capture and plays the negotiated 16/24 kHz response, then resumes listening. Press OK again to close the session, stop the answer and release resources. Capture and playback own separate encoder/decoder phases and release buffers/BSP resources between them; the microphone is paused during replies. There is no local wake word. Certificate-verified MQTT/TLS + AES-CTR UDP or WebSocket/TLS (frame versions 1–3) remains bounded against malformed/truncated frames, stale UDP sequences and unrelated sessions. Network/reply waits are bounded; network errors require manual retry. Closing waits for worker cleanup. Remote firmware/system-control commands are ignored and MCP/device-control capability is not advertised.

Nine transparent 128×128 I4 expression frames occupy 74,304 bytes in application Flash, down 83.2% from the former 128×128 RGB565A8 pack (442,368 bytes). The character is centered at native size. A shared fifteen-color palette plus a transparent entry keeps frame colors consistent. LVGL expands a single 512-byte ARGB row at a time, with decoder bookkeeping and allocator overhead, instead of allocating a full image. Whole-image RAM loading is disabled and guarded at compile time. Native tests decode every row of all nine frames, check visible/transparent pixels, require less than 1 KiB transient pool use and verify full cleanup. Listening/thinking have dedicated expressions; idle blinks; LLM emotion events select happy, concerned or surprised accents. Talking-mouth frames run only near successful PCM output and stop when audio stops; this is a voice-activity animation, not phoneme lip synchronization. UTF-8 subtitles use the existing font/fallback handling. Activation codes and credentials are not logged. A separate NVS namespace retains only a stable client UUID; temporary connection keys/tokens are zeroed when the session is released. Leaving the app waits for transport callbacks, sockets, queues, codecs, DMA, buffers and its task to stop before removing the UI. Before connecting, the feed worker cooperatively stops and its 8 KiB stack is freed; exit recreates it while retaining cached data and refresh requests. The voice worker uses a measured 28 KiB stack, following an observed SILK stack-protection fault with 8 KiB. Bootstrap/message JSON buffers are temporary and freed before audio capture or after message parsing. App and page names are only XiaoZhi; the start-conversation hint appears only in the footer. Existing music/radio, stored settings and home images remain compatible. Physical activation, microphone/speaker quality, volume, interruption and repeated-switch memory stability require device testing; host mocks do not prove cloud compatibility.

Application state, models, worker tasks, UI, networking, and persistence are under `main/pocket`; the reusable BSP audio API offers a playback-only mode to omit unused microphone DMA. Button callbacks only enqueue events. LVGL access stays under the BSP lock. The audio worker owns HTTP/MP3/I2S; a separate feed worker fetches weather/radio. A shared mutex serializes their HTTP/TLS allocations on this no-PSRAM board, and background requests pause while settings is open or audio is active.

Audio resources follow the active track: completion, failure, cancellation and switching all close HTTP/TLS and MP3, free input/output buffers, suspend the codec and deinitialize its interfaces and I2S channels/DMA. The shared I2C bus remains available for the battery gauge. The 10,240-byte playback task is created on demand and deleted after its cleanup acknowledgement when leaving an app or switching tracks; manual switches and playlist advances complete this stop before changing the selection and starting the next track. A timeout or failed release keeps the current page/selection for retry and never forcibly deletes an active producer. Pausing keeps the current stream for resume. Page changes already close the image file/frame and delete the previous LVGL objects; closing settings stops its HTTP server and SoftAP. Shared Wi-Fi/display/feed services and small weather/timer state remain available across navigation. Physical repeated-switch memory stability still requires device testing.

The pinned MP3 layer-3 decoder lazily allocates 27,840 bytes of working memory before allocator overhead. The application reserves 26 KiB for LVGL (native stress peak 18,544 bytes) and six static Wi-Fi RX buffers, matching the default six-frame Block Ack window. Compared with the earlier 32 KiB/ten-buffer settings, this returns about 12 KiB to the system heap so verified HTTPS and MP3 decoding can coexist. TLS verification and audio task stack size remain enabled/unchanged. Decode errors log the codec return code and memory state; the first successful PCM write logs its format and memory state.

Button volume updates use a separate `volume` NVS byte, loaded over the legacy config value, to avoid copying or rewriting the playlist during playback. Phone configuration saves update both values. The audio worker applies changes between PCM writes, retaining exclusive codec ownership; pauses apply the latest value when resumed. Each long press changes one step (no auto-repeat).

The old `config`, Wi-Fi, and image-slot keys remain compatible. A separate versioned `profile` NVS blob stores company, city, offset, and manual stations. Uploads use two files, CRC/length/delay checks, and publish only the verified inactive image. Only one 18,432-byte image frame stays in RAM. The settings service retains its per-boot WPA2 password, bounded forms/uploads, and token-protected write routes; secrets are neither returned nor logged.

## Build and validation

Install and activate ESP-IDF 5.5.3 using the [environment guide](development/engineering/environment-setup.md).
Use Node.js 20 or later for the web sources and tests.

```bash
cd web
npm ci
npm run build
cd ..
./tools/validate.sh
cmake -S tests/pocket -B build/host-ui
cmake --build build/host-ui -j 8
build/host-ui/pocket-render main/pocket/ui.c main/pocket/player.c main/pocket/model.c main/main.c main/pocket/feeds.c
```

The complete gate runs upstream checks, application model tests, JavaScript frame-pack
tests, embedded-web consistency checks, a firmware build, merged-image validation,
and a content-addressed debug archive. Native rendering additionally checks every
non-ASCII character in the listed device UI sources against the actual LVGL font,
a known-missing negative case, and twelve complete navigation cycles plus maximum-length fields with the same 26 KiB pool configured for firmware. The native test requires at least 4 KiB of usable headroom after its peak.
Images from that test are written to `build/home.ppm`, `build/player.ppm`, and `build/settings.ppm`.

### Local visual preview

The [FoloToy Passport Simulator](https://github.com/VOID001/FoloToy-Passport-Simulator)
runs the actual ESP32-C3 merged image. In that repository:

```bash
npm run prepare:emulator
npm start
```

Open `http://127.0.0.1:4190`, select local firmware, and upload
`build/FoloToy-AI-Passport-full.bin` from this application. Click the device buttons
or use the arrow keys and Enter. Hold a button for about one second for a long press.
The simulator does not reproduce the battery gauge or a complete phone-to-SoftAP
provisioning path, and it cannot establish real-board audio quality or power usage.

To inspect the phone editor separately without a physical device:

```bash
python3 tools/pocket/preview.py
```

Open `http://127.0.0.1:4191`. This is the real bundled editor with a development-only
in-memory API. Changes affect only the local test session. Image uploads are saved
under `build/browser-upload.pfi` for firmware-format checks; Wi-Fi is not connected.
This preview is not evidence that the device HTTP service or provisioning worked.

## Firmware and stored data

| Partition | Offset | Size |
| --- | --- | --- |
| NVS | `0x9000` | `0x6000` |
| PHY | `0xF000` | `0x1000` |
| Factory application | `0x10000` | `0x4F0000` |
| SPIFFS images | `0x500000` | `0x300000` |

The verified full image includes a fresh SPIFFS volume and is intended for writing
at `0x0`. It replaces the installed firmware and resets image storage; NVS settings
may also be reset. This is not a preserve-data upgrade. Export any needed data
before approving a flash. No full-chip erase or original-firmware readback is required.
A failed filesystem mount is reported rather than formatting existing images automatically.

The debug archive under `build/firmware/<SHA256>/` binds the merged image to its ELF
and MAP. The additional `storage.bin` is included in the merged image; retain it
separately if segmented flashing is needed. Never write the application-only image at `0x0`.

Report Build, Host tests, Simulator tests, Device tests, and Unverified separately.
Physical acceptance still needs Wi-Fi success/failure/restart, interrupted uploads,
play/pause/switch/stop under weak networking, speaker output, repeated resource cleanup,
Chinese rendering, and battery/power measurements. Flashing always requires explicit
approval for the exact firmware and device.

## XiaoZhi / Furina validation, 2026-10-22

Build / Host tests: PASS, complete ESP-IDF 5.5.3 gate and verified archive, 480 production-worker mock WebSocket/MQTT-UDP paths including activation/cancellation/resource failures, bounded JSON/framing/session checks and the existing music/radio/BSP regressions. Native UI: PASS, twelve nine-page navigation cycles, all nine expression frames, activation-code rendering, fixed glyphs and maximum fields; peak 18,544 / 23,016 usable bytes (26,624 reserved). Protocol mocks use fake codecs/TLS and local UDP sockets; they do not establish real cloud or audio behavior.

Full SHA256 `62eede774d84bbf75cdafe2ff8cfa6f922661fae723deabde1960271b481bb90`; matching ELF SHA256 `897c6d44aec5cc53b7a1b4c8af9c222afb321efa02327b99fdc08e8ee544e7a4`. Bundle: `build/firmware/62eede774d84bbf75cdafe2ff8cfa6f922661fae723deabde1960271b481bb90/`. Application: 3,990,944 bytes, written range `0x10000`–`0x3de59f`. After user authorization, the fresh live-device table matched and the application-only write/hash verification passed. Device tests: PASS for flashing and a 45-second startup observation only: one matching-ELF boot, Wi-Fi reconnected and no crash markers. The serial monitor is closed. Pending: official activation, microphone/Opus, audible quality/volume/interruption and repeated voice/music/radio heap stability. NVS and image partitions were not written. Device evidence: `build/device-xiaozhi-test-20261022.json`. Record: `build/xiaozhi-validation-record.json`; local preview: `build/furina-ui.png`.

## App and track resource cleanup, 2026-10-22

Audio stop now waits for producer cleanup, releases I2S/codec interfaces and DMA, and deletes the idle playback task. Manual track switches and natural playlist advances stop before changing selection or opening the next stream; app exits use the same barrier. Failed suspend/channel release and stop timeout remain retryable without forced deletion. Pause keeps the stream for resume. UI/image and settings-server teardown is retained; common services remain available.

Build / Host tests: PASS, complete gate, 360 playback/error/retry paths plus 41 cancellation cases, 20 BSP deinitialize/reinitialize cycles, release-failure recovery, and native Chinese-font/navigation regression (18,504 / 23,032 usable bytes). Full SHA256 `154a7040406565a819691b200ea7140385ce73ba5953b562f33c0925378350a2`; matching ELF SHA256 `37b442994a802309afb72407552f2222cab2d2ba795ea9d0be15523aaa45a555`. Bundle: `build/firmware/154a7040406565a819691b200ea7140385ce73ba5953b562f33c0925378350a2/`. Planned application-only range: `0x10000`–`0x35608f`. Device tests: NOT RUN; not flashed. This supersedes the unflashed headroom-only build below, and includes its playback-only/TLS memory changes. Actual radio playback, repeated-switch heap stability and audio quality remain unverified. Record: `build/audio-lifecycle-record.json`.

## Playback memory headroom, 2026-10-22

Build / Host tests: PASS, complete gate and supplemental output-only constructor rollback test. Effective generated settings confirm peer retention off, certificate bundle on, insecure TLS off and 16 KiB inbound records. Full SHA256 `609353b4de60252653df26f3e646c131df2211e911127affe074d6882d3c36d7`; matching ELF SHA256 `674e4497bb27298cd50a9d53f45810cebf40b665fe68a329e4c7e6e92ec120b1`. Bundle: `build/firmware/609353b4de60252653df26f3e646c131df2211e911127affe074d6882d3c36d7/`. Planned application-only range: `0x10000`–`0x355cdf`. Device tests: NOT RUN; not flashed. The bounded passive capture received no radio playback attempt, so no radio-specific allocation failure was reproduced. The monitor is closed. Record: `build/radio-memory-headroom-record.json`.

After the user reported radio memory exhaustion, inspection confirmed that this playback-only application was also allocating the microphone RX channel. The player now requests a DAC/TX-only BSP mode, with the same six TX descriptors and 240 frames. This omits an RX DMA payload of 5,760 bytes in the initial stereo format, or 2,880 bytes after mono reconfiguration, plus RX driver/descriptor overhead. These are source-derived estimates, not measured device savings. The default full-duplex BSP mode remains available to capture applications; callers choose one mode before initialization, and sleep/wake preserves it. Host tests cover both modes, twenty output-only format/sleep/wake cycles, volume retention, mode mismatch, read rejection and failure recovery.

The TLS configuration also disables retention of the peer certificate after normal verification. ESP-IDF documents an approximate 4 KiB heap saving, depending on the chain. The pinned mbedTLS implementation verifies the chain before releasing it and retaining the digest/public key required by the handshake. The application does not inspect peer certificates. Certificate-bundle verification, hostname checks and the 16 KiB inbound TLS limit remain in place. TLS 1.3 was not enabled and is not added by this change. Radio-specific reproduction and actual memory savings remain unverified; the current device still runs the earlier loop build.

## Playlist looping and device update, 2026-10-22

Music starts with OK, advances only after successful natural completion, and wraps the last track to the first; a single-track playlist repeats. The controller updates the selected track/title and resets progress through the existing start path after cleanup. Empty/invalid lists, errors, pause and other pages never advance. A pause pressed at EOF is retained until resume; radio keeps its existing behavior. No new worker or audio buffer is added.

Build / Host tests: PASS, complete gate plus the updated EOF-pause runtime regression, ordered advance/wrap/single/empty/error/page policy tests, 360 player lifecycle runs and native Chinese UI/navigation stress (18,504 / 23,032 usable bytes, 26,624 reserved). Full image SHA256 `9e743bc5fe0f9c14b9e469fe08ec463778529c69b9124900dba8524f6fc1b624`; matching ELF SHA256 `51c28d6c18abef4a93c90bd822dc0ad791f5365856350b34a2acb96c260b5d87`. Bundle: `build/firmware/9e743bc5fe0f9c14b9e469fe08ec463778529c69b9124900dba8524f6fc1b624/`.

The user authorized the combined loop/volume/memory iteration. The application-only image was written at `0x10000`–`0x355BBF` on `/dev/cu.usbmodem101` after fresh device-table comparison, with written-data hash verification; NVS and images were not written. Device tests: PASS for the flash and 45-second startup observation (one matching-ELF boot, Wi-Fi reconnected, no crash markers). Passive runtime capture then confirmed PCM output at 44.1 kHz, five live volume updates from 35% to 60%, natural completion with automatic index 0 to 1, and PCM output of the next source at 48 kHz with volume 60%. No public decode failure or crash occurred in this capture. Five MP3 initialization allocation failures recovered during the next start; its first PCM had 6,760 free bytes and a largest block of 2,304 bytes. This remains a tight memory margin, not proof of a complete memory fix. Physical audible quality, volume-down, last-to-first wrap and prolonged stability remain unverified. The bounded serial monitor is closed. Record: `build/playlist-loop-validation-record.json`.

## Intermittent MP3 allocation failure, 2026-10-22

Build / Host tests: PASS. Full image SHA256 `6b59d31a637673db521fe6582567149c77bfb75efce252a4d83fd867079c7ca6`; matching ELF SHA256 `2a9b041cee13909bb5ebee534c53979c54087191225a12958388b7e6079bde2d`. Bundle: `build/firmware/6b59d31a637673db521fe6582567149c77bfb75efce252a4d83fd867079c7ca6/`. Planned application-only range: `0x10000`–`0x355a7f`; the table matches the last verified device read. Device tests for this iteration: NOT RUN.

Passive serial observation reproduced decoder initialization return 10 / public return -2. Before processing, free heap was 40,816 bytes and the largest block 23,552 bytes; cleanup recovered to 78,096 bytes versus 78,100 before HTTP. This confirms allocation failure, without evidence of a sustained leak in that attempt; the exact failed request and fragmentation versus the peak after the first network read remain unresolved. No live ELF identity appeared in this capture, and the device was not reset.

Playback now copies only the selected URL instead of allocating a 4,968-byte playlist configuration. The feed worker checks playback/settings before allocating its 4,208-byte profile, allocates under the shared HTTP mutex, and frees it before handoff; manual offline stations remain supported. Audio cleanup holds that mutex through buffer/decoder release and codec sleep. A nonallocating audio-task heap hook records the last failed size/capabilities, and a probe measures heap immediately before the first decode. Volume controls, TLS certificate verification, partitions and UI memory settings are retained.

The actual player implementation passed 360 synthetic success/failure/retry runs, checking that no HTTP/decoder/buffer/awake-codec resources survive network handoff. Native glyph/render/navigation stress passed at 18,504 / 23,032 usable UI bytes (26,624 reserved). These checks do not establish that the intermittent device failure is resolved. This iteration is not flashed; repeated playback/switching and any failed-allocation probe remain pending. Local record: `build/music-memory-lifecycle-record.json`.

## Button volume controls, 2026-10-22

Build / Host tests: PASS. Full image SHA256 `709764f69aa1af0330ade2b76ce3a4c8b4deeee4998eeff5293ae641fdeed375`; ELF SHA256 `837718732ae2a9650d15638617dd4e22e95e54c2e6b6826af1d724404eb4a6fc`. Bundle: `build/firmware/709764f69aa1af0330ade2b76ce3a4c8b4deeee4998eeff5293ae641fdeed375/`. Application size: 3430832 bytes, planned offset `0x10000`; the archived table matches the last device read. Device tests: NOT RUN. Record: `build/volume-validation-record.json`.

Music and radio now use long UP/DOWN for one 5-point volume step per hold, clamped to 0–100%, with the current percentage visible on both pages. Short presses still switch sources; long OK returns to the app list, where long DOWN opens settings. Volume is shared with the phone editor and survives restart. A dedicated NVS byte avoids playlist allocations during playback; only the audio worker touches the codec. Native font/layout and twelve-cycle navigation stress passed at 18,504 / 23,032 usable UI bytes (26,624 reserved), including 0% and 100% on both audio pages. On-device button response, audible volume changes and reboot persistence remain unverified; this firmware has not been flashed.

## MP3 decoder memory repair, 2026-10-22

- Real-device reproduction with the user-selected Hao Hao source: verified HTTPS opened; before MP3 processing, free heap was 20,296 bytes and the largest block was 8,192 bytes. The pinned decoder logged `Fail to init MP3 decoder ret 10`; its allocation-failure branch returns 10, and its layer-3 working allocations total 27,840 bytes before allocator overhead. This establishes memory exhaustion rather than an invalid audio URL.
- Reduced the LVGL reserve from 32 to 26 KiB and static Wi-Fi RX buffers from ten to six (matching the effective six-frame RX BA window), reclaiming about 12 KiB. TLS verification, audio stack, storage layout and saved configuration are retained. Decoder failure and first successful PCM output now include bounded diagnostics.
- Build/host tests: PASS, full gate, embedded-assets check, firmware/archive verification and native font/render regression. Twelve navigation cycles plus maximum-length saved fields peaked at 17,984 of 23,040 usable bytes (26,624 reserved), retaining more than 4 KiB of usable UI headroom. Effective firmware values were verified as LVGL 26 KiB, static RX six and RX BA window six.
- Full SHA256 `14c607dfbf4a76e232d450fdbc80301c9ad6f3847346c05dbe8ef97904d0d52a`; ELF SHA256 `b8ce3f6f946794de89ce5ad0732b28bbf69fcbefbf00b1e251401e449d74ba38`. Bundle: `build/firmware/14c607dfbf4a76e232d450fdbc80301c9ad6f3847346c05dbe8ef97904d0d52a/`. The application spans `0x10000`–`0x35570F`; the partition table matches a fresh live-device read. Device tests: PASS for the authorized application-only flash, written-data hash verification and 45-second startup observation (one boot with matching ELF, Wi-Fi reconnected, no crash markers). NVS and image partitions were not written; the serial monitor is closed. Record: `build/device-mp3-memory-test-20261022.json`. Pending: on-device decode/PCM progress, audible output and weak-network memory stability. The user subsequently confirmed audible OSS playback. Diagnosis: `build/mp3-memory-diagnosis.json`.

## Audio source update, 2026-10-22

The user replaced the third-party music source with four MP3 objects in their own OSS bucket: OAOA, Hao Hao, Almost Famous, and Turning. Updated the existing [playlist JSON](https://jumpserver.oss-cn-hangzhou.aliyuncs.com/media/mayday-playlist.json) with percent-encoded UTF-8 filenames and `Cache-Control: no-cache`; retained the old JSON locally. Anonymous range reads and host MP3 probes passed for all four, and the editor accepted the updated playlist. Re-import and save it on the device to replace its stored track list; no new firmware is needed. The external-source TCP timeout was observed at 1,806 ms. The subsequent eight-second diagnostic build was abandoned at the user's request, its source changes were reverted and it was never flashed; the device remains on the previously verified `e310d8dd...` build. Audible playback with the new URLs remains unverified.

## Validation record: playlist import update, 2026-10-22

- Added URL/local-file playlist import to the existing phone music editor, prefilled the supplied OSS playlist, and removed coordinate inputs. Preset cities resolve locally; new custom cities use name-based weather geocoding. Existing settings/image formats and partition layout are retained.
- Build and host tests: PASS, complete gate, 10 JavaScript tests, embedded-asset consistency, merged-image validation and archive verification. Full SHA256 `e310d8dd532e5d32cfb8b7084a2f988ee7e16cd749c82ad48f838babdc50b871`; matching ELF SHA256 `a57a8d8f70b0465a683225d8cc37b0d6c109c5f4eab3a75e9af76be9ca4050e6`. Bundle: `build/firmware/e310d8dd532e5d32cfb8b7084a2f988ee7e16cd749c82ad48f838babdc50b871/`.
- Isolated browser preview: PASS for six-song OSS/local-file imports, invalid-URL draft preservation, save/reload order and typed Tianjin coordinates. A live host lookup also resolved Suzhou; these checks do not establish device HTTP/TLS or weather behavior.
- Device tests: PASS for the user-authorized app-only update on `/dev/cu.usbmodem101`, matching live partition table, data-hash verification and 45-second startup observation: one matching ELF boot, Wi-Fi reconnected, no crash markers. App data spans `0x10000`–`0x35557F`, affected sectors through `0x355FFF`; NVS/image partitions were not written. Serial monitor closed. Local evidence: `build/device-playlist-test-20261022.json`.
- Unverified: physical phone-to-device playlist fetching/import/save, custom-city weather and audible playback. Stored tracks are changed only when the user imports and saves the playlist in the new editor; flashing does not replace the existing music configuration.

## Validation record: Pocket Passport 0.3.1, 2026-10-22

- Final layout: upper card/name/company and weather icons; lower 26-week heatmap only, with larger squares and no GitHub title, week label or error text. Missing data shows the built-in decorative default; phone settings explain it is a public ibuler snapshot, not live account statistics.
- Build: PASS, complete gate and merged/debug-archive verification. Full SHA256 `1508cbe4e7c01a59777023c1a7dd3f26c743fafc2d113010d903c99df868b784`; matching ELF SHA256 `92a59232f48997de6ca4affb358a71f7f99211e3a0a36eff32d947fe50982799`. Archive: `build/firmware/1508cbe4e7c01a59777023c1a7dd3f26c743fafc2d113010d903c99df868b784/`; package: `build/Pocket-Passport-0.3.1-firmware.zip`. The merged image flashes at `0x0` and resets image storage/may reset NVS.
- Host tests: PASS, complete gate, parser ASan/UBSan, real public calendar parsing, nonempty fallback checks, Chinese-font checks, eight-page rendering for twelve cycles and eight weather icon states. LVGL peak 17,800 / 32,768 bytes. Native live/default previews: `build/home-v031.png` and `build/home-default-v031.png`; browser default/validation/save/reload checks use an isolated preview session.
- Diagnosis: initial physical fetch returned HTTP 200 and the correct UTC day, but the eight-second body limit stopped at 202,752 of 225,625 bytes. The parser had not failed. The matching diagnostic archive is `7ef28c486bd829a2ee5af2ca3df6e8b1587e14b494a1add831d35b424551b263`; stack minimum was 2,500 bytes, pre-HTTP free heap 54,704 bytes and largest block 23,552 bytes. Increased the bounded body budget to fifteen seconds and cancel when audio/settings need the HTTP resource.
- Device: the live partition table matched; the automatically authorized update writes only app data `0x10000` through `0x3547BF` (affected sectors through `0x354FFF`), preserving NVS and image storage. Device tests: PASS for flashing/hash verification, one matching 0.3.1 boot over 45 seconds, Wi-Fi/IP reconnect and successful GitHub HTTP 200 retrieval/parsing of all 225,625 bytes. No crash markers; minimum stack free 2,376 bytes, pre-fetch free heap 53,136 bytes and largest block 26,624 bytes. Serial monitor closed. Evidence: `build/device-v031-test-20261022.json`.
- Unverified: physical screen acceptance, GitHub ID persistence through device reboot, long-running fetch/audio resource behavior and speaker output. The earlier silent-audio issue remains unresolved.

## Validation record: Pocket Passport 0.3.0, 2026-10-22

- Changes: compact upper card/name/company layout; lower GitHub contribution panel with default `ibuler` and independent phone settings; drawn weather icons on home and weather pages. BSP, old configuration/profile blobs and image format are preserved.
- Build: PASS, complete gate and merged/debug-archive verification. Full SHA256 `eeb077b9adeb288381c540488e5aff42933740625a459d31ab8cd8d9862f1ea8`; matching ELF SHA256 `49a3bc68db087bcadb3442456fd2bca3ec03290913dc112f480fb9497faed8e1`. Archive: `build/firmware/eeb077b9adeb288381c540488e5aff42933740625a459d31ab8cd8d9862f1ea8/`; package: `build/Pocket-Passport-0.3.0-firmware.zip` (full image at `0x0` resets image storage/may reset NVS).
- Host tests: PASS, complete repository gate, ASan/UBSan calendar parser tests, live public ibuler HTML (119 active days in the displayed window), Chinese-font checks, eight-page rendering over twelve cycles (17,856 / 32,768-byte LVGL peak), and all eight weather icon states. Browser verified invalid ID rejection, saving another ID, reload, and restoration to ibuler in the isolated preview session. Native preview is `build/home-v030.png`.
- Device tests: the continued automatic-flashing instruction applies. Exact live partition-table comparison passed; app-only write starts at `0x10000`, data through `0x35460F`, affected sectors through `0x354FFF`; NVS and images are outside that range. Startup/network evidence is recorded separately in `build/device-v030-test-20261022.json`.
- Unverified: physical layout/icon acceptance, ID persistence through device reboot, long-running fetch/audio resource behavior and speaker output. The prior silent-audio issue remains unresolved.

## Validation record: Pocket Passport 0.2.2, 2026-10-22

- Change: removed the separate filled card background that formed colored strips above and below the square image. Image placement, name/company positions and the single home footer remain unchanged.
- Build: PASS, complete gate, merged-image verification and debugging archive verification. Full SHA256 `ad8ff14a46580886af1505583493cfbbef1402b6477ddee6f3b5046a48709f92`; matching ELF SHA256 `a939f74174d321eef51f8c088159d33e9316e0b64950ee635455dbc1f96a5b0b`. Archive: `build/firmware/ad8ff14a46580886af1505583493cfbbef1402b6477ddee6f3b5046a48709f92/`; package: `build/Pocket-Passport-0.2.2-firmware.zip` (merged image at `0x0`; resets image storage/may reset NVS).
- Host tests: PASS, complete repository checks and native Chinese-font/eight-page rendering, twelve cycles, LVGL peak 17,816 / 32,768 bytes. Render inspection confirmed the card background strips are absent.
- Device tests: PASS for flashing/hash verification and 45-second startup observation: one matching 0.2.2 boot and no crash markers; the serial monitor was closed. Evidence is in `build/device-v022-test-20261022.json`. The previous automatic-flashing instruction applies to this continued UI correction. Use only the verified application at `0x10000` (affected sectors through `0x352FFF`) with the matching live partition table to preserve NVS and images.
- Unverified: physical appearance of the retained card image. The earlier silent-audio issue remains unresolved.

## Validation record: Pocket Passport 0.2.1, 2026-10-22

- Changes: taller card area, name/company below it, one home footer line, no home weather cache/update status, and Tianjin in phone city presets. Existing square image format and saved data remain compatible.
- Build: PASS, complete repository gate and merged-image/archive verification. Full SHA256 `467ffda9c021038ea7b3351d787900652641d12abf09e3ff00706fecf37b6d32`; matching ELF SHA256 `428fd1560fcb57e8d06b0459e2e4c73f18e0a25d3e329e6f2605e4ab03eb06be`. Archive: `build/firmware/467ffda9c021038ea7b3351d787900652641d12abf09e3ff00706fecf37b6d32/`. Package: `build/Pocket-Passport-0.2.1-firmware.zip`; full image is flashed from `0x0` and resets image storage/may reset NVS.
- Host tests: PASS, complete gate, source/embedded web consistency, native Chinese-glyph checks and eight-page rendering over twelve cycles (LVGL pool peak 17,840 / 32,768 bytes). Local browser confirmed Tianjin, automatic coordinates and saving in the isolated preview session.
- Device: the user explicitly authorized automatic flashing after completion. The live partition table matched; wrote only the verified application at `0x10000` through `0x35294F` (affected sectors through `0x352FFF`) on `/dev/cu.usbmodem101`. Esptool verified the written hash; NVS and image partitions were not written. Device tests: PASS for flashing/hash verification and a bounded 45-second startup observation: one 0.2.1 boot, matching ELF identity, no panic/assertion/watchdog/reboot loop. The serial monitor was closed. Evidence is in `build/device-v021-test-20261022.json`.
- Unverified: physical layout/Chinese acceptance, retained image display, Tianjin weather/radio service availability, and audio output. The earlier silent-playback issue remains unresolved.

## Validation record: Pocket Passport 0.2, 2026-10-22

- Build: PASS, complete `./tools/validate.sh` with ESP-IDF 5.5.3. Application size: 3,418,592 / 5,177,344 bytes. Full image: 8,388,608 bytes, SHA256 `11aae2b1078ba9de74cd5c8d369da41d4a19728f513abd19f46586196597d673`; matching ELF SHA256 `d0a0310b8a4509fc2fa4a808dbf9560bea2240b150c0b34e8fe6c41aaea4560a`. Bundle: `build/firmware/11aae2b1078ba9de74cd5c8d369da41d4a19728f513abd19f46586196597d673/`. Verified ZIP: `build/Pocket-Passport-firmware.zip`.
- Host tests: PASS, repository gate plus sanitized model tests for profile bounds, city selection, monotonic pause/resume/reset, JSON nesting limits, and native font/render checks. Eight pages were switched twelve times; peak LVGL pool use was 17,848 / 32,768 bytes. Offline clock placeholders, negative temperatures, and persistent radio-error text were checked.
- Browser editor: company/city/manual radio settings saved and survived reload in the isolated preview API. Host live requests returned the expected Open-Meteo fields and four MP3/non-HLS directory entries for the sample city. This does not validate those public streams' audio output.
- Simulator: the exact production code with a documented data-only fixture overlay displayed the color GIF card, company, synchronized clock, and live weather. The long-OK launcher, city-filtered manual radio fixture, local MP3 PCM progress, pause/resume, return-to-launcher, and a focus countdown from 25:00 to 24:31 were observed. The microphone and audible browser output stayed disabled. The overlay is never a physical flashing artifact.
- Device tests: PASS for the separately approved application-only update and startup-log check, with physical feature acceptance pending. The live device partition table matched the verified layout before writing. Wrote the exact application at `0x10000` through `0x3529DF` (affected sectors through `0x352FFF`); esptool verified its data hash. NVS and image partitions were not written. A subsequent 45-second capture showed one Pocket Passport 0.2.0 startup, the matching ELF identity, initialized display/LVGL/battery/buttons, retained Wi-Fi reconnecting, and no panic, assertion, watchdog, or reboot loop. The serial monitor was closed. Raw evidence stays local in `build/device-v2-flash-20261022.log` and `build/device-v2-boot-20261022.log`. The user subsequently confirmed that retrying the card-image upload displayed the image on the home screen. Screen/button behavior, company/city configuration, public radio, speaker output, long-running resources, and power still await full acceptance; the earlier silent-audio report remains unresolved.
- The new partition table is byte-identical to the previously flashed 0.1 image. With separate approval, the matching application-only image (SHA256 `96e5f320a4d12e176dcc9447f79a34b9f27a9dfc6ff41e69b5725e77b52411f6`) can be written at `0x10000` through `0x3529DF` (affected sectors through `0x352FFF`) while retaining NVS and SPIFFS. Do not use this segmented update on an unknown layout; do not write it at `0x0`. The full image remains available for intentional complete refresh.

## Historical validation: Pocket Focus 0.1, 2026-10-21

- Build: PASS, ESP-IDF 5.5.3; the full repository gate verified all four image offsets.
- Host tests: PASS; upstream checks, six frame-pack tests, model tests, actual-font coverage, and native UI rendering. The 32 KiB LVGL pool peaked at 23,984 bytes during twelve navigation cycles.
- Simulator: home, GIF loop, Chinese labels, OK navigation, long-DOWN settings, fixed-length 44.1 kHz stereo MP3 playback, pause at eight seconds, resume to thirty seconds, 22.05 kHz mono chunked playback to twelve seconds, invalid-audio errors, and recovery back to valid playback were observed. These audio fixtures were generated locally and routed only to a dedicated loopback test server.
- The default Internet HTTPS track did not pass simulator playback: it reported a time-synchronization failure. TLS verification remained enabled. This remains unverified on real hardware.
- The phone-editor browser test produced a six-frame color image and a one-frame grayscale image; both passed the C format/CRC check. A malformed GIF with an oversized frame was rejected before decoding, and a subsequent valid GIF could still be uploaded. Its API was the isolated preview, not the device SoftAP.
- Device tests: NOT RUN. USB discovery found only host Bluetooth/debug ports. Physical Wi-Fi provisioning, HTTP upload failure recovery, audio quality, power, battery, and heap stability remain unverified.

The simulator preview is a data-only overlay of the verified application:

```bash
# Run inside the activated ESP-IDF environment.
python tools/pocket/prepare_simulator.py --image build/animation.pfi --tone-tests --card-test
# Generate local test tones with ffmpeg, then start this instead of npm start:
node tools/pocket/simulator_fixture.mjs ../FoloToy-Passport-Simulator build/tone.mp3 build/tone-mono.mp3
```

The helper requires ESP-IDF 5.5.3. Its JSON sidecar records the production and preview hashes and confirms that code bytes are unchanged. The test image and generated tones live only under ignored `build/`; the physical-device artifact has a blank image volume and the original example track. Stop the ordinary simulator server before starting the fixture server on the same port. `.nvmrc` selects Node 24 for users of nvm.

## Historical device record: Pocket Focus 0.1, 2026-10-22

- With explicit user approval, flashed the verified production image to `/dev/cu.usbmodem101` from `0x0` through `0x7FFFFF` (8 MiB). SHA256: `75cec4c626b04440daf520d71f3a60bff87cfe73089c47e30c637f2ab5e36c35`. No rebuild or separate full-chip erase was performed. Esptool identified an ESP32-C3 revision 1.1 with 8 MB embedded Flash and verified the written-data hash.
- Reset once and captured 45 seconds of serial output. The application/ELF identity matched the archive; display, LVGL, CW2017 battery gauge, Wi-Fi, and buttons initialized. The settings SoftAP/DHCP service started at `192.168.4.1`. One application startup and no panic, assertion, watchdog, or reboot loop were observed in this window. Initial RF calibration data was absent after the refresh; full calibration completed and saved new data.
- Device tests: PASS for flashing/hash verification and the bounded startup-log check. The user additionally confirmed the home/Chinese display, OK entry into the player, and long-DOWN entry into settings. Subsequent logs showed HTTPS certificate validation and a successful 44.1 kHz codec open. The user reported a playing state without audible output; audio acceptance is not passed and its cause is still under investigation. Image upload/animation, provisioning persistence, speaker output/quality, power, and long-running resource stability remain pending. Serial observation was stopped and the port released.
- Local raw evidence: `build/device-flash-20261022.log` and `build/device-boot-20261022.log`; do not publish unsanitized device logs.

## XiaoZhi stack-protection repair, 2026-10-22

The previously flashed `62eede77…` connected successfully, then faulted in `silk_encode_frame_FIX` / `silk_Encode` during initial microphone encoding. The crash ELF prefix matched `897c6d44aec5…`; the stack pointer was below its 8 KiB bounds. Increasing the stack requires reclaiming background-task and retained JSON memory together. Host validation covers real worker cleanup and 100 feed stop/late-acknowledgement/recreate/OOM cycles, but codec stack headroom and cloud audio require the repaired image on hardware. Transparent frame rendering and removal of duplicate prompts pass the native renderer.

Repair build and host tests: PASS, complete ESP-IDF 5.5.3 gate. Full image SHA256 `facdb42fa68b36b546eabd47aa5e99eabcd97bbdaeefdefc482ec2b6bd541695`; matching ELF SHA256 `395dced065bcb37627ebceaa94a2388a4c927b91950e243b27edd5cd3e165776`. Verified bundle: `build/firmware/facdb42fa68b36b546eabd47aa5e99eabcd97bbdaeefdefc482ec2b6bd541695/`. Application size: 4,139,616 bytes, offset `0x10000`, last byte `0x402a5f`. Record: `build/xiaozhi-fix-validation-record.json`; transparent preview: `build/xiaozhi-transparent-ui.png`.

Device startup: PASS, fresh live partition table matched, application-only write/hash verified, one matching-ELF boot and Wi-Fi reconnection over 45 seconds without crash markers. NVS and image partitions were not written. Voice acceptance remains pending device interaction; startup success does not establish encoding/audio stability.

Voice acceptance of `facdb42f…` failed: transport connected, but Opus initialization returned -7 with 45,212 free heap bytes and a largest block of 18,432 bytes. The user confirmed transparency but requested a softer lower portrait edge. The subsequent repair uses the reference Opus task size of 24 KiB, records real encode/PCM high-water marks and refines the silhouette.

Stack sizing reference: [XiaoZhi Opus task at commit 7e69845](https://github.com/78/xiaozhi-esp32/blob/7e69845d818553ac045efbcde746b22bef6eecf6/main/audio/audio_service.cc#L147-L154), 12 × 2048 bytes. This is a starting configuration; acceptance depends on measured stack headroom with this pinned codec and target.

The revised portrait uses a curved, softly fading alpha silhouette at the bust base, retaining nine expressions and the same Flash-backed RGB565A8 layout; native renderer peak remains 18,544 / 23,016 usable bytes. Source: `assets/images/furina/furina-sheet-soft.png`; preview: `build/xiaozhi-soft-ui.png`.

Revised build and host tests: PASS, complete ESP-IDF 5.5.3 gate. Full SHA256 `ddca4b4f0a4f20c4d3230e690f852c0215b73b34bfbb8896aadc0f235c9bbd79`; matching ELF SHA256 `cfcbcda6d3a29f8b5c1e707c4fe086ce350afb3716dc518960cde1f54cc298b9`. Verified bundle: `build/firmware/ddca4b4f0a4f20c4d3230e690f852c0215b73b34bfbb8896aadc0f235c9bbd79/`. Application: 4,139,664 bytes at `0x10000`–`0x402a8f`. Record: `build/xiaozhi-fit-validation-record.json`.

On-device attempts with `ddca4b4f…` stopped before configuration: TLS select/handshake timed out twice. This did not establish an activation/binding failure, and encoding/response headroom remains unverified. The next change raises bootstrap and transport connection timeouts from 2.5 to 8 seconds, retries one failed/truncated network bootstrap with cleanup between attempts, preserves client identity and separates network, service, memory, invalid-configuration and activation-pending messages. Host regression adds transient success, persistent failure, service rejection and truncated response cases (540 voice paths). The portrait moves down 20 pixels; status moves above it while subtitles and footer retain their space.

Device error notices are limited to twelve Unicode characters including punctuation. Built-in voice errors use shorter wording; the shared UTF-8 notice copier enforces the boundary without splitting characters. Voice errors, player errors, weather errors and controller notices use this path, while conversational subtitles retain their normal length. Host tests cover Chinese punctuation, ASCII, emoji, invalid/control text and small byte buffers; native rendering covers an oversized error notice.

Current repair Build / Host tests: PASS, full gate and verified archive. Full SHA256 `0d38298c9a170e1b8df29d7bd120ff0f41c1985b704144aca3442ef94ea0056d`; matching ELF SHA256 `3e2e93027867be470a5acafcb01d380f6a433925e2e45adf9729db70212b75e9`. Bundle: `build/firmware/0d38298c9a170e1b8df29d7bd120ff0f41c1985b704144aca3442ef94ea0056d/`. Application: 4,140,256 bytes at `0x10000`–`0x402cdf`. Record: `build/xiaozhi-notice-validation-record.json`. Device acceptance is recorded separately after the authorized application-only write.

On `0d38298c…`, real capture/Opus encoding and PCM output succeeded and the user confirmed audible replies. First-encode stack margin was 2,952 bytes, falling to 440 bytes after a longer recording; the next image adds 4 KiB and requires heap/stack retesting. The user additionally requested continuous conversation, implemented with server `auto` mode and STT/TTS phase transitions, listening again after replies and using OK to open/close.

Continuous-mode acceptance uses the [pinned control protocol](https://github.com/78/xiaozhi-esp32/blob/7e69845d818553ac045efbcde746b22bef6eecf6/docs/websocket.md#L150-L165). Host mocks cover two automatic turns, STT-to-thinking, TTS-start without STT, response timeout, second-turn encoder/buffer failure, and OK-close/retry in four active phases (620 voice paths). Hardware is silenced before network abort/disconnect; the next listen phase waits at least 500 ms after both last PCM and TTS stop.

Continuous build / host tests: PASS, complete gate and verified archive. Full SHA256 `b50808c4fd56bc5c6a77ca4e5ac40117caedfa6056036bb4c4616aa821549e5f`; matching ELF SHA256 `87ec3b89e15f2f1c22dce7c5b9d8e23dd4046a8538e07df2e9867da3fd4698b8`. Bundle: `build/firmware/b50808c4fd56bc5c6a77ca4e5ac40117caedfa6056036bb4c4616aa821549e5f/`; application 4,139,936 bytes at `0x10000`–`0x402b9f`. Device tests are recorded separately in `build/xiaozhi-continuous-validation-record.json`.

Continuous-image device startup: PASS, fresh live table match, application-only write/hash verified and one matching-ELF boot with Wi-Fi reconnection over 45 seconds. NVS/images were not written. A subsequent 300-second passive listener captured no XiaoZhi operation and is closed. Continuous voice acceptance and 28 KiB codec stack/heap headroom are NOT RUN; the prior manual-image audible response does not validate this new mode.

## Continuous recording allocation order, 2026-10-22

The `b50808c4…` device connected with 54,196 bytes free but only a 20,480-byte largest block; Opus initialization failed with -7. Creation now reserves a 25 KiB encoder block and the 28 KiB worker stack before allocating the session context. The worker waits for a notification until context ownership is established, replaces the reservation with the encoder before bootstrap/TLS and retains it into the first capture. Microphone/I2S initialization still happens only when listening starts. Creation failures free the reservation and delete only the not-yet-notified task; normal exits retain the cooperative cleanup barrier. Host tests cover allocation ordering, notification ownership, partial context/reservation failure, two continuous turns and phase cleanup (660 paths). Voice errors appear below the portrait, clear stale subtitles and retain the twelve-character limit.

Allocation-order repair Build / Host tests: PASS. Verified bundle `build/firmware/16533a41e7d05ff5416f0bfe384b17c2fa3cd3e91918832f3c89cca516799a1e/`; full SHA256 `16533a41e7d05ff5416f0bfe384b17c2fa3cd3e91918832f3c89cca516799a1e`, matching ELF SHA256 `d2dc82916fbb5b1f37a2ea16ea1cbb35107ff8df6672bd96f90836564824018a`. Application 4,140,208 bytes at `0x10000`–`0x402caf`. Device evidence is recorded separately in `build/xiaozhi-order-validation-record.json`.

Device follow-up: application-only flashing/hash verification and matching-ELF startup passed. The user initially reported recording startup failure after several turns; the failure itself was not captured completely. A second, uninterrupted passive capture on the same firmware recorded eight replies and nine successful capture starts, and the user confirmed normal operation. Capture free heap stayed within 23,244–23,520 bytes, largest block was 13,824 bytes and voice-task stack minimum was 4,560 bytes. No encoder failure, session cleanup or crash appeared in that capture. The serial monitor is closed. Host coverage now runs twenty replies per successful continuous session across both transports. This bounded success does not resolve the earlier intermittent failure; extended operation, repeated app switching and physical OK-close acceptance remain unverified. No further firmware was flashed for this follow-up. Local evidence: `build/xiaozhi-repeat-device-test.json`.

## Playback buffering and memory budget

Music and radio use a separate 6 KiB-stack HTTP reader and fixed 4 KiB compressed-byte FIFO. Decoding starts after 3 KiB, or after a shorter stream completes; an empty FIFO triggers rebuffering. At 128 kbit/s a full FIFO represents about 256 ms of audio, less at higher bitrates. The connection/header budget is 8 seconds and normal reads use 1.8 seconds. Failed connections report a network error; cancellation is checked after opening and before reading headers. Sustained slow sources can still stall.

Playback pauses the 8 KiB-stack feed worker, offsetting most of the reader/FIFO cost: the nominal task/buffer increase is 2 KiB before allocator and task overhead. The priority-5 PCM worker runs above the priority-4 reader and LVGL tasks. Teardown joins the reader before freeing its FIFO, HTTP connection or stack-owned context. Explicit stop resumes feeds; natural completion retains the player until the controller advances or exits. Feed pause/resume failures remain retryable. Diagnostics report connection timing, FIFO underruns, maximum PCM-feed gap excluding intentional pauses, reader stack headroom and heap state.

XiaoZhi stops playback before allocating its existing 28 KiB voice stack and 25 KiB encoder reserve. If stack-first allocation fails, it fully releases that attempt and retries reserve-first once. The voice worker is notified only after all session allocations succeed. Diagnostics distinguish stack, encoder-reserve and context failures. This bounds retries and ownership; it cannot guarantee allocation when no sufficiently large block exists.

The LCD uses one 240×20 RGB565 DMA buffer instead of 40 lines, saving 9,600 bytes. Heatmap pixels are allocated only on home and released after deleting the image object and dropping its cache entry. Linker heap-boundary and boot heap-region comparisons showed 5,760 fewer static RAM bytes. Together these changes are expected to provide 15,360 extra heap bytes outside home before other allocations; actual free-heap and largest-block gains require device measurement. Optional heatmap allocation failure does not prevent navigation.

Output-only I2S uses six 480-frame DMA descriptors, allowing up to 60 ms of mono output at 48 kHz instead of 30 ms. This adds 2,880 bytes of mono DMA payload, or 5,760 bytes during initial stereo-slot setup, released before voice. Full-duplex capture retains 240-frame descriptors. I2S interrupts remain active during cache-disabled Flash writes. TLS certificate verification, codec stacks, saved settings and partition layout are preserved.

The home animation reuses one 96×96 RGB565 frame buffer (18,432 bytes); more frames increase storage, not that buffer. XiaoZhi's nine 128×128 I4 portraits use 74,304 Flash bytes and one 512-byte decode row plus bookkeeping. The former 442,368-byte RGB565A8 portrait pack was also Flash-backed, so shrinking it primarily saves Flash and does not establish an animation leak.

## Current validation and hardware acceptance

- Build: PASS, complete ESP-IDF 5.5.3 gate and merged-image/archive verification.
- Host tests: PASS, including concurrent FIFO wrap/slow reads/EOF/cancellation/allocation failures, both voice allocation orders, BSP DMA/buffer contracts, source-image size boundaries and embedded assets. Native UI passes ASan/UBSan, twelve navigation cycles, all nine expressions, optional-image allocation failure and repeated cleanup. Portrait decoding uses less than 1 KiB transient pool memory and retains none after close; peak LVGL usage is 18,504 / 22,736 usable bytes from a 26 KiB reserve.
- Device tests: PASS for application-only writing, readback verification, a 45-second matching-ELF startup with Wi-Fi/IP reconnection and no crash markers, and user-confirmed 128×128 portrait size/rendering. The live partition table matched before writing; NVS, images, bootloader and partition table were not written. The serial monitor is closed.
- Unverified: voice-linked portrait animation, sustained music/radio quality, repeated multi-track-to-voice transitions, live heap/largest-block gains and display refresh performance. Earlier playback trials had stutter/crackles and intermittent voice allocation errors; the current changes have not yet been shown to resolve those behaviors.

The tested full-image SHA256 is `fef1c1113b60ebff02581eaa8e5d9efbfe405d88b2442f0741863bb4173ba72a`; matching ELF SHA256 is `a4a8541949118a3f6881abc203d9fa7ac60506222b6d1b06851fded1ceb2b1a7`. The application is 3,777,360 bytes at `0x10000`–`0x3aa34f`, with affected sectors through `0x3aafff`. The verified bundle is `build/firmware/fef1c1113b60ebff02581eaa8e5d9efbfe405d88b2442f0741863bb4173ba72a/`; local records are `build/huolinger-large-validation.json` and `build/huolinger-large-device-test.json`. A merged write at `0x0` may reset stored data. A future data-preserving application-only update requires explicit approval and a fresh live partition-table comparison.
