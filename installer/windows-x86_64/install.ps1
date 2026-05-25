# =============================================================================
# WS-PluginManager Installer for Windows (x86_64)
# =============================================================================
#
# Supports:
#   - Installing v.1.0.1 (current release)
#   - Detecting an already-installed version
#   - Upgrading and uninstalling
#
# Requires Wireshark 4.6.x (Windows binary is built against WS 4.6.x).
#
# Plugin directory:
#   Personal:  %APPDATA%\Wireshark\plugins\<version>\epan\
#   System:    C:\Program Files\Wireshark\plugins\<version>\epan\
#
# Usage:
#   Run install.bat   (recommended -- keeps the window open)
#   or: .\install.ps1
# =============================================================================

$ErrorActionPreference = "Stop"

$ScriptDir      = Split-Path -Parent $MyInvocation.MyCommand.Path
$PluginName     = "ws_pluginmgr.dll"
$CurrentVersion = "1.0.1"

Write-Host ""
Write-Host "===========================================================" -ForegroundColor Cyan
Write-Host "      WS-PluginManager Installer for Windows               " -ForegroundColor Cyan
Write-Host "      x86_64 (64-bit Intel/AMD)                            " -ForegroundColor Cyan
Write-Host "      v.1.0.1  --  requires Wireshark 4.6.x               " -ForegroundColor Cyan
Write-Host "===========================================================" -ForegroundColor Cyan
Write-Host ""

# --- Warn if launched directly (e.g. double-click) ---
$parentProcess = (Get-CimInstance Win32_Process -Filter "ProcessId=$PID" -ErrorAction SilentlyContinue).ParentProcessId
$parentName    = (Get-Process -Id $parentProcess -ErrorAction SilentlyContinue).ProcessName
if ($parentName -notmatch '^(cmd|powershell|pwsh|WindowsTerminal)$') {
    Write-Host "  !! IMPORTANT: Run this installer from a Command Prompt window !!" -ForegroundColor Red
    Write-Host "     If you double-clicked this file you may miss interactive"      -ForegroundColor Yellow
    Write-Host "     prompts and the window may close before you can read them."    -ForegroundColor Yellow
    Write-Host ""
    Write-Host "     How to run correctly:"                                          -ForegroundColor White
    Write-Host "       1. Open Command Prompt  (search: cmd)"                       -ForegroundColor White
    Write-Host "       2. cd /d `"$ScriptDir`""                                     -ForegroundColor White
    Write-Host "       3. install.bat"                                               -ForegroundColor White
    Write-Host ""
    Write-Host "     Continuing anyway in 5 seconds..." -ForegroundColor Yellow
    Start-Sleep -Seconds 5
    Write-Host ""
}

# =============================================================================
# PREREQUISITES CHECK
# =============================================================================
Write-Host "Checking prerequisites..." -ForegroundColor White
Write-Host ""

# --- OS Detection ---
$osBuild = [System.Environment]::OSVersion.Version.Build
$osName  = if ($osBuild -ge 22000) { "Windows 11" } elseif ($osBuild -ge 10240) { "Windows 10" } else { "Windows (older)" }
$osArch  = if ([System.Environment]::Is64BitOperatingSystem) { "x86_64 (64-bit)" } else { "x86 (32-bit)" }
Write-Host "  OS              : " -NoNewline; Write-Host "$osName  build $osBuild  $osArch" -ForegroundColor Cyan
if (-not [System.Environment]::Is64BitOperatingSystem) {
    Write-Host "  [WARN] This installer is for 64-bit Windows only." -ForegroundColor Red
}

# --- VC++ Runtime ---
$vcFound   = $false
$vcVersion = $null
foreach ($kp in @(
    "HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64",
    "HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\X64"
)) {
    if (Test-Path $kp) {
        try {
            $vcVersion = (Get-ItemProperty $kp -ErrorAction SilentlyContinue).Version
            if ($vcVersion) { $vcFound = $true; break }
        } catch {}
    }
}
if ($vcFound) {
    Write-Host "  VC++ 2022 x64   : " -NoNewline; Write-Host "[FOUND] $vcVersion" -ForegroundColor Green
} else {
    Write-Host "  VC++ 2022 x64   : " -NoNewline; Write-Host "[NOT FOUND] Required for WS-PluginManager to load" -ForegroundColor Red
    Write-Host "                    Install from: https://aka.ms/vs/17/release/vc_redist.x64.exe" -ForegroundColor Yellow
}

# --- Verify plugin binary ---
Write-Host ""
Write-Host "  Plugin binary in this installer:"
$PluginFile = Join-Path $ScriptDir "v.$CurrentVersion\$PluginName"
$binaryOk = $false
if (Test-Path $PluginFile) {
    $sz = [math]::Round((Get-Item $PluginFile).Length / 1KB)
    Write-Host "    v.$CurrentVersion\$PluginName : " -NoNewline; Write-Host "[FOUND]  ($sz KB)" -ForegroundColor Green
    $binaryOk = $true
} else {
    Write-Host "    v.$CurrentVersion\$PluginName : " -NoNewline; Write-Host "[MISSING]" -ForegroundColor Yellow
}

if (-not $binaryOk) {
    Write-Host ""
    Write-Host "  Error: Plugin binary not found in this installer package." -ForegroundColor Red
    Read-Host "  Press Enter to exit"; exit 1
}

# --- Detect Wireshark ---
Write-Host ""
Write-Host "  Searching for Wireshark:"
$WsVersion     = $null
$WiresharkPath = $null

foreach ($path in @(
    "$env:ProgramFiles\Wireshark",
    "${env:ProgramFiles(x86)}\Wireshark",
    "$env:LOCALAPPDATA\Programs\Wireshark"
)) {
    if (Test-Path "$path\Wireshark.exe") {
        Write-Host "    $path\Wireshark.exe : " -NoNewline; Write-Host "[FOUND]" -ForegroundColor Green
        $WiresharkPath = $path; break
    } else {
        Write-Host "    $path\Wireshark.exe : " -NoNewline; Write-Host "[not found]" -ForegroundColor DarkGray
    }
}

if ($WiresharkPath) {
    try {
        $vi = (Get-Item "$WiresharkPath\Wireshark.exe").VersionInfo
        $WsVersion = "$($vi.FileMajorPart).$($vi.FileMinorPart).$($vi.FileBuildPart)"
        Write-Host "    Version from EXE metadata: " -NoNewline; Write-Host $WsVersion -ForegroundColor Cyan
    } catch {}
}

if (-not $WsVersion) {
    $ts = Get-Command "tshark" -ErrorAction SilentlyContinue
    if ($ts) {
        try {
            $out = & tshark --version 2>&1 | Select-Object -First 1
            if ($out -match '(\d+\.\d+\.\d+)') { $WsVersion = $Matches[1] }
        } catch {}
    }
}

if (-not $WsVersion -and $WiresharkPath) {
    $tsharkExe = "$WiresharkPath\tshark.exe"
    if (Test-Path $tsharkExe) {
        try {
            $out = & $tsharkExe --version 2>&1 | Select-Object -First 1
            if ($out -match '(\d+\.\d+\.\d+)') { $WsVersion = $Matches[1] }
        } catch {}
    }
}

if (-not $WsVersion) {
    Write-Host ""
    Write-Host "  [WARN] Could not detect Wireshark version automatically." -ForegroundColor Yellow
    $inp = Read-Host "  Enter Wireshark major.minor version (e.g., 4.6)"
    $WsVersion = "$inp.0"
}

$WsMajor = $WsVersion.Split('.')[0]
$WsMinor = $WsVersion.Split('.')[1]

if ($WsMajor -ne "4" -or $WsMinor -ne "6") {
    Write-Host ""
    Write-Host "  [WARN] This binary was built against Wireshark 4.6.x." -ForegroundColor Yellow
    Write-Host "         Your version: $WsVersion -- the plugin may fail to load." -ForegroundColor Yellow
    $force = Read-Host "  Install anyway? [y/N]"
    if ($force -ne "y" -and $force -ne "Y") {
        Write-Host "Installation cancelled."
        Read-Host "Press Enter to exit"; exit 1
    }
}

# --- Determine plugin directory ---
Write-Host ""
Write-Host "  Searching for plugin directory:"
$foundPathId = $null
$searchBases = @(
    $(if ($WiresharkPath) { "$WiresharkPath\plugins" } else { $null }),
    "$env:APPDATA\Wireshark\plugins",
    "$env:LOCALAPPDATA\Wireshark\plugins"
) | Where-Object { $_ }

foreach ($base in $searchBases) {
    if (Test-Path $base) {
        foreach ($d in (Get-ChildItem $base -Directory -ErrorAction SilentlyContinue)) {
            $match = $d.Name -match "^$($WsMajor)[\.\-]$($WsMinor)$"
            $color = if ($match) { "Green" } else { "DarkGray" }
            Write-Host "    $base\$($d.Name)  $(if ($match) {'[MATCH]'} else {''})" -ForegroundColor $color
            if ($match -and -not $foundPathId) { $foundPathId = $d.Name }
        }
    }
}

$PluginPathId = if ($foundPathId) { $foundPathId } else { "$($WsMajor).$($WsMinor)" }
if ($foundPathId) {
    Write-Host "    => Using plugin path ID: " -NoNewline; Write-Host $PluginPathId -ForegroundColor Cyan
} else {
    Write-Host "    => No existing version directory found; will use default: " -NoNewline
    Write-Host $PluginPathId -ForegroundColor Yellow
}

$PersonalPluginDir = "$env:APPDATA\Wireshark\plugins\$PluginPathId\epan"
$SystemPluginDir   = if ($WiresharkPath) { "$WiresharkPath\plugins\$PluginPathId\epan" } else { $null }

# --- Detect currently installed version ---
Write-Host ""
Write-Host "  Checking for existing WS-PluginManager installation:"
$InstalledVersion = $null
$InstalledPath    = $null

foreach ($dir in (@($PersonalPluginDir, "$env:LOCALAPPDATA\Wireshark\plugins\$PluginPathId\epan", $SystemPluginDir) | Where-Object { $_ })) {
    $candidate = "$dir\$PluginName"
    if (Test-Path $candidate) {
        Write-Host "    $($candidate) : " -NoNewline; Write-Host "[FOUND]" -ForegroundColor Green
        $InstalledPath = $candidate
        try {
            $bytes = [System.IO.File]::ReadAllBytes($InstalledPath)
            $text  = [System.Text.Encoding]::ASCII.GetString($bytes)
            if ($text -match 'ws_pluginmgr (\d+\.\d+\.\d+)') {
                $InstalledVersion = $Matches[1]
                Write-Host "    Embedded version: " -NoNewline; Write-Host "v.$InstalledVersion" -ForegroundColor Cyan
            }
        } catch {}
        break
    } else {
        Write-Host "    $($candidate) : " -NoNewline; Write-Host "[not found]" -ForegroundColor DarkGray
    }
}
if (-not $InstalledPath) {
    Write-Host "    No existing installation found." -ForegroundColor DarkGray
}

# --- Summary ---
Write-Host ""
Write-Host "-----------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "  Prerequisites Summary" -ForegroundColor White
Write-Host "-----------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "  OS              : $osName  build $osBuild  $osArch"
Write-Host "  VC++ 2022 x64   : " -NoNewline
if ($vcFound) { Write-Host "OK ($vcVersion)" -ForegroundColor Green } else { Write-Host "NOT FOUND  (required)" -ForegroundColor Red }
Write-Host "  Wireshark       : " -NoNewline
if ($WiresharkPath) { Write-Host "Found at $WiresharkPath" -ForegroundColor Green } else { Write-Host "Not found in standard locations" -ForegroundColor Yellow }
Write-Host "  Wireshark ver   : " -NoNewline; Write-Host "$WsVersion  (plugin API: $PluginPathId)" -ForegroundColor Cyan
Write-Host "  v.1.0.1 binary  : " -NoNewline
if ($binaryOk) { Write-Host "present" -ForegroundColor Green } else { Write-Host "not available" -ForegroundColor Yellow }
Write-Host "  Installed now   : " -NoNewline
if ($InstalledVersion) { Write-Host "v.$InstalledVersion  at $InstalledPath" -ForegroundColor Cyan } else { Write-Host "None" -ForegroundColor DarkGray }
Write-Host "-----------------------------------------------------------" -ForegroundColor DarkGray

# --- Main menu ---
Write-Host ""
Write-Host "What would you like to do?"
Write-Host ""
if ($InstalledPath) {
    if ($InstalledVersion -eq $CurrentVersion) {
        Write-Host "  i) Reinstall v.$CurrentVersion" -ForegroundColor Green
    } else {
        Write-Host "  i) Upgrade to v.$CurrentVersion  (installed: v.$InstalledVersion)" -ForegroundColor Green
    }
} else {
    Write-Host "  i) Install v.$CurrentVersion" -ForegroundColor Green
}
Write-Host "  u) Uninstall" -ForegroundColor Red
Write-Host "  q) Quit"      -ForegroundColor Yellow
Write-Host ""
$action = Read-Host "Choice [i]"
if (-not $action) { $action = "i" }

switch ($action.ToLower()) {
    "u" {
        if (-not $InstalledPath) {
            Write-Host "`nWS-PluginManager is not currently installed." -ForegroundColor Yellow
            Read-Host "Press Enter to exit"; exit 0
        }
        Write-Host "`nRemove: $InstalledPath" -ForegroundColor Cyan
        $confirm = Read-Host "Confirm uninstall? [y/N]"
        if ($confirm -eq "y" -or $confirm -eq "Y") {
            Remove-Item $InstalledPath -Force
            Write-Host "`nWS-PluginManager uninstalled successfully." -ForegroundColor Green
        } else {
            Write-Host "Uninstall cancelled."
        }
        Read-Host "Press Enter to exit"; exit 0
    }
    "q" { Write-Host "Bye."; Read-Host "Press Enter to exit"; exit 0 }
    { $_ -eq "i" -or $_ -eq "" } { }
    default { Write-Host "Invalid choice." -ForegroundColor Red; Read-Host "Press Enter to exit"; exit 1 }
}

# --- Choose install location ---
Write-Host ""
Write-Host "Where would you like to install?"
Write-Host ""
Write-Host "  1) Personal directory (recommended -- survives Wireshark updates)" -ForegroundColor Green
Write-Host "     $PersonalPluginDir" -ForegroundColor Gray
if ($SystemPluginDir) {
    Write-Host ""
    Write-Host "  2) System directory (wiped on Wireshark updates)" -ForegroundColor Yellow
    Write-Host "     $SystemPluginDir" -ForegroundColor Gray
}
Write-Host ""
$locChoice = Read-Host "Choice [1]"
if (-not $locChoice) { $locChoice = "1" }
$InstallDir = if ($locChoice -eq "2" -and $SystemPluginDir) { $SystemPluginDir } else { $PersonalPluginDir }

# --- Install ---
Write-Host ""
Write-Host "Installing to: $InstallDir" -ForegroundColor Cyan
if (-not (Test-Path $InstallDir)) { New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null }
Copy-Item $PluginFile "$InstallDir\$PluginName" -Force
try { Unblock-File "$InstallDir\$PluginName" -ErrorAction SilentlyContinue } catch {}

# --- Done ---
if (Test-Path "$InstallDir\$PluginName") {
    Write-Host ""
    Write-Host "===========================================================" -ForegroundColor Green
    Write-Host "      Installation successful!                             " -ForegroundColor Green
    Write-Host "===========================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Installed:  WS-PluginManager v.$CurrentVersion" -ForegroundColor Cyan
    Write-Host "  Location:   $InstallDir\$PluginName"
    Write-Host ""
    Write-Host "  Next steps:"
    Write-Host "  1. Restart Wireshark (if running)"
    Write-Host "  2. Open Tools menu -- Plugin Manager appears at the bottom"
    Write-Host "  3. Click 'Manage Plugins...' to open the plugin browser"
    Write-Host ""

    if (-not $vcFound) {
        Write-Host "  [WARN] VC++ 2022 Redistributable (x64) was not detected." -ForegroundColor Red
        Write-Host "         If WS-PluginManager fails to load, install it from:"  -ForegroundColor Yellow
        Write-Host "         https://aka.ms/vs/17/release/vc_redist.x64.exe"       -ForegroundColor Yellow
        Write-Host ""
    }

    Write-Host "  To uninstall, run this script again and choose 'u'."
    Write-Host ""
} else {
    Write-Host "Error: Installation failed." -ForegroundColor Red
    Read-Host "Press Enter to exit"; exit 1
}

Read-Host "Press Enter to exit"
