# WS-PluginManager v.1.0.1 — Release Notes

**Released:** 2026-05-25
**Status:** Stable
**Requires:** Wireshark 4.0.x – 4.6.x

---

## What Changed

v.1.0.1 is a bugfix and polish release. No new features; the plugin remains feature-complete.

### Dark Mode Fix

The restart banner was unreadable on macOS dark theme — white system text rendered on a hardcoded light-yellow background. Both the C-plugin banner (amber) and the new Lua banner (green) now use explicit palette-aware colour pairs and re-apply automatically when the OS switches between Light and Dark mode at runtime.

### Restart / Reload Buttons

| Situation | What you see now |
|---|---|
| **C plugin toggled** | Amber banner: *"⚠️ One or more C plugins changed — restart Wireshark to apply."* + **Restart Now** button + **Later** dismiss |
| **Lua plugin toggled** | Green banner: *"✓ Lua plugin reloaded — changes are active immediately."* + **OK** dismiss |

**Restart Now** spawns a fresh Wireshark instance (`QProcess::startDetached`) and exits the current one cleanly — no manual quit-and-relaunch needed after toggling a compiled plugin.

### Installer Improvements

All three platform installers (macOS, Linux, Windows) now:

- **Detect existing installs in all directories** — personal user directory *and* all system plugin directories. Previously only the personal directory was checked, so system-installed copies were missed.
- **Show context-aware menu labels** based on what is found:
  - `Install v.1.0.1` — nothing installed
  - `Reinstall v.1.0.1` — same version already present
  - `Upgrade to v.1.0.1  (installed: v.X.X.X)` — older version found
- **Fixed silent crash on existing-install detection** (Linux / macOS) — the `strings | grep | grep` pipeline would exit the script silently under `set -e` when no version string was found. Added `|| true` guard.

---

## Download & Install

### macOS (Universal Binary — Intel + Apple Silicon)

```bash
cd installer/macos-universal
chmod +x install.sh
./install.sh
```

### Linux (x86_64 — WS 4.0 / 4.2 / 4.4 / 4.6)

```bash
cd installer/linux-x86_64
chmod +x install.sh
./install.sh
```

### Windows (x86_64 — WS 4.6)

```
cd installer\windows-x86_64
install.bat
```

---

## Supported Platforms

| Wireshark | macOS Universal | Windows x86_64 | Linux x86_64 |
|---|---|---|---|
| 4.6.x | ✓ | ✓ | ✓ |
| 4.4.x | — | — | ✓ |
| 4.2.x | — | — | ✓ |
| 4.0.x | — | — | ✓ |

---

*AI-Assisted: yes (Claude by Anthropic) — cross-platform installers, cross-version build system, documentation.*
