# TeamTalk Pro Native - Qt parity tracker

This file is the acceptance checklist for replacing the Qt Windows client with the maintained native Windows client.

## Baseline and safety

- [x] Keep `master` unchanged while native work is validated.
- [x] Keep a Qt backup branch.
- [x] Build `TeamTalk5Classic` against the current TeamTalk SDK.
- [x] Validate that the native EXE has no Qt runtime dependency.
- [x] Preserve/import the existing TeamTalk Pro Qt settings on first native start.
- [ ] Merge into the official TeamTalk Pro client only after native validation passes.

## TeamTalk Pro custom features

- [x] Microphone EQ: bass, mid and treble.
- [x] Secondary sound input device.
- [x] Import TeamTalk Pro Qt sound/EQ settings.
- [x] Import the option controlling timestamps in messages.
- [x] TeamTalk Pro GitHub Releases updater runtime.
- [x] Native TeamTalk Pro audio settings window.
- [x] TeamTalk Pro menu inside the native client.
- [ ] Move all remaining Pro-only settings from auxiliary dialogs into the main Preferences experience.
- [ ] Audit every TeamTalk Pro-only Qt setting and add an equivalent native setting.

## Native Windows UI modernization

- [x] Use current Windows theme for tree/list/tab controls.
- [x] Keep standard Win32/MFC controls instead of adding a new heavy GUI framework.
- [x] Add keyboard-accessible TeamTalk Pro menu commands.
- [ ] Improve DPI scaling and modern font/layout defaults without breaking accessibility.
- [ ] Review every dialog for focus order, labels and keyboard access.
- [ ] Replace obsolete bitmap-era presentation where it can be done without reducing usability.
- [ ] Review dark/high-contrast/system-theme behavior.

## Current Qt client feature parity audit

### General / connection
- [ ] Compare every General preference with the Qt client.
- [ ] Compare every Connection preference with the Qt client.
- [ ] Join Code support.
- [ ] Current auto-connect/reconnect/auto-join behavior.
- [ ] Current BearWare.dk login behavior.
- [ ] Current Windows Firewall preference behavior.

### Channels / users / administration
- [ ] Compare all current channel properties and codec controls.
- [ ] Compare current classroom/free-for-all/transmit controls.
- [ ] Compare user accounts, bans, kick/ban and operator controls.
- [ ] Compare current online users and server statistics behavior.
- [ ] Compare current user/channel sorting and display options.

### Audio
- [ ] Current WASAPI/default communications-device selection behavior.
- [ ] Current input/output device UID restoration behavior.
- [ ] Current AGC, denoise and echo-cancellation behavior.
- [ ] Current audio preprocessor controls.
- [x] TeamTalk Pro secondary input.
- [x] TeamTalk Pro microphone EQ.
- [ ] Verify all audio behavior against the Qt client with real devices.

### Chat / display
- [ ] Current chat-history behavior.
- [ ] Current timestamp formatting controls.
- [x] Preserve TeamTalk Pro timestamp enable/disable setting during migration.
- [ ] Chat templates.
- [ ] Current status-bar event configuration.
- [ ] Current channel topic/server-name/user-info display options.

### Sound events
- [ ] Compare every current Qt sound event.
- [ ] Sound-pack selection.
- [ ] Per-event custom WAV files.
- [ ] Playback mode: default / one-by-one / overlapping.
- [ ] Current event volume and output-device behavior.

### Accessibility / TTS / keyboard
- [x] Native MSAA/Windows accessibility remains enabled.
- [x] Prism support is built into the native client when available.
- [ ] Current Prism backend selection.
- [ ] Speech/Braille output mode selection.
- [ ] Current customizable TTS event messages and variables.
- [ ] Current shortcut list and every Qt shortcut/hotkey.
- [ ] First-run accessibility behavior from the current Qt client.

### Video / desktop / media
- [ ] Compare current video capture devices/formats/codecs.
- [ ] Compare desktop sharing and desktop input behavior.
- [ ] Compare local media playback behavior.
- [ ] Channel media stream pause/resume and current stream controls.
- [ ] Current media codecs/formats supported by the Windows client.

### Updates / release packaging
- [x] Native client has TeamTalk Pro updater runtime.
- [ ] Native installer/portable package becomes the release payload.
- [ ] Existing Qt TeamTalk Pro clients are offered the native update only after validation.
- [ ] Preserve a rollback path to the last known-good Qt build.

## Validation gates

A feature is only marked complete after:
1. It compiles in `Windows Native Client` diagnostics workflow.
2. The EXE remains free of Qt runtime dependencies.
3. Existing server connection/channel/user/audio behavior is not regressed.
4. Keyboard navigation and accessibility are tested.
5. Settings survive restart and migration from the Qt TeamTalk Pro client when applicable.
