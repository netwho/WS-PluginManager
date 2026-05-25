# WS-PluginManager — Changelog

All notable changes to WS-PluginManager are documented here.

---

## v.1.0.1 — 2026-05-25

### Fixed
- **Dark mode banner** — restart / reload banners were unreadable on macOS dark theme (white text on a light-yellow background). Both banners now adapt to the OS theme with explicit palette-aware colour pairs, and re-apply automatically if the user switches Light ↔ Dark at runtime.

### Added
- **"Restart Now" button** on the C-plugin banner — spawns a fresh Wireshark instance and exits the current one cleanly. No need to manually quit and relaunch after toggling a compiled plugin.
- **"Later" button** on the C-plugin banner — dismisses the warning without restarting.
- **Lua reload banner** — a green info banner appears whenever a Lua plugin is toggled, confirming "changes are active immediately." Dismisses with OK. Adapts to dark/light theme alongside the C banner.

### Improved
- **Installer — existing-install detection** (all platforms): previously only checked the personal user directory. Now scans personal *and* all system directories, printing `[FOUND]` / `[none]` for each, matching the behaviour already present in the macOS installer.
- **Installer — context-aware menu** (all platforms): the install menu now shows the correct action based on what is found:
  - *No installation found* → `Install v.1.0.1`
  - *Same version already installed* → `Reinstall v.1.0.1`
  - *Older version found* → `Upgrade to v.1.0.1  (installed: v.X.X.X)`
- **Installer — `set -e` + grep crash fixed** (Linux / macOS): the `strings | grep | grep` pipeline for reading an embedded version string would silently exit the script when no match was found. Added `|| true` guard.

---

## v.1.0.0 — 2026-05-24

Initial public release. Feature-complete; future v.1.x.x releases will be bugfixes and compatibility updates only.

### Features
- **Plugin list** — name, language (C / Lua), type (dissector · tap · filetype · codec), version, directory
- **Enable / disable** — toggle a checkbox; file is renamed `foo.so` ↔ `foo.so.disabled`. Non-destructive and fully reversible.
- **Lua instant reload** — Lua plugins re-register immediately on toggle; no restart needed.
- **C plugin banner** — toggling a compiled plugin shows a restart reminder.
- **Profiles** — save, load, and delete named profiles; each stores the full enabled/disabled state of every plugin.
- **Search / filter** — real-time filter by name, language, or type.
- **Open User Dir** — opens the personal plugin folder in the system file manager.
- **Delete** — permanently remove a plugin file with confirmation.
- **Cross-platform** — macOS Universal Binary (Intel + Apple Silicon), Linux x86_64 (WS 4.0 / 4.2 / 4.4 / 4.6), Windows x86_64 (WS 4.6).
