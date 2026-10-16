# CrateDig

**Dig YouTube for samples and drag them straight into your DAW.**
An AU / VST3 instrument plugin inspired by [samplette.io](https://samplette.io): hit **DIG** for a random
crate-digging search (or type your own), audition what you find, chop the part you like and drag it onto a track.

**You decide what gets downloaded.** A search only lists results. Audio is fetched only when you press **Listen**,
into a temporary preview cache. It reaches your sample library only when you press **Download** or drag a chop out.

![CrateDig](docs/cratedig-medium.png)

## Features

- **DIG**: random samplette-style searches ("1974 jazz funk rare", "1960s bossa nova 45"...) from a genre and decade,
  or a free-text **search** in the YouTube search box
- **Results / History** tabs with thumbnails, durations and view counts. Shorts, reviews, "top 100" videos and
  anything outside 45 s to 15 min are filtered out
- **Listen**: fetches a temporary preview and plays it through the plugin. Any MIDI note retriggers the chop.
  A dot in the list marks previews that are already cached
- **Chop**: drag on the waveform to select a region, drag its edges to adjust, click to play from a point, double-click to clear
- **BPM & key detection** on every preview
- **Download**: saves the chop (or the whole track) as a 24-bit WAV in `~/Music/CrateDig` and reveals it.
  Pressing it before Listen fetches and saves in one go
- **Drag to DAW**: drag the tile onto a track; the same WAV is written and dropped
- **Resizable window**: drag the corner (760×500 up to 2200×1400) or use the S / M / L presets; the size is saved per instance
- Restoring a project never touches the network. A preview reloads only if it is still in the local cache

| Small | Large |
|---|---|
| ![small](docs/cratedig-small.png) | ![large](docs/cratedig-large.png) |

## Requirements: yt-dlp

CrateDig drives **your own install of [yt-dlp](https://github.com/yt-dlp/yt-dlp)**. It does not ship it.

```bash
brew install yt-dlp
```

On Windows: `winget install yt-dlp.yt-dlp`, plus `winget install Gyan.FFmpeg` (needed there to decode the audio).

CrateDig looks for yt-dlp in Homebrew, `/usr/local/bin`, `~/.local/bin`, WinGet/Scoop folders and `PATH`. If it lives
elsewhere, click **yt-dlp...** in the header to point at it (remembered for all instances).
Optional extras that are picked up automatically when installed:

- **ffmpeg**: previews are converted to WAV. Without it, macOS decodes YouTube's m4a natively
- **deno**: yt-dlp's recommended JavaScript runtime for YouTube; reduces extraction failures

Keep yt-dlp up to date (`brew upgrade yt-dlp`). YouTube changes often, and an outdated yt-dlp is the most common cause of errors.

## Install the plugin

Download the latest build from [Releases](../../releases) (or the *Actions* tab artifacts), then:

| Format | Copy to |
|---|---|
| AU (`CrateDig.component`) | `~/Library/Audio/Plug-Ins/Components/` |
| VST3 (`CrateDig.vst3`) | `~/Library/Audio/Plug-Ins/VST3/` (macOS) · `C:\Program Files\Common Files\VST3\` (Windows) |

The macOS builds are ad-hoc signed, not notarised. If macOS blocks them, run:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/CrateDig.component ~/Library/Audio/Plug-Ins/VST3/CrateDig.vst3
```

CrateDig is an **instrument**: load it on an instrument/MIDI track so the preview keeps running when the transport is stopped.

## Build from source

Requirements: CMake ≥ 3.22, a C++20 compiler (Xcode / Command Line Tools on macOS, Visual Studio 2022 or newer on Windows). JUCE is fetched automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/CrateDigTests_artefacts/Release/CrateDigTests
```

Plugins land in `build/CrateDig_artefacts/Release/{AU,VST3,Standalone}`. Extra targets:

- `CrateDigSmoke [path/to/yt-dlp] [search text]`: real end-to-end run (search, preview, analysis, WAV)
- `CrateDigSnapshot docs --dig [path/to/yt-dlp]`: renders the editor to PNG (configure with `-DCRATEDIG_BUILD_SNAPSHOT=ON`)

## How it works

```
DIG / Search ─► Digger thread ─► yt-dlp --flat-playlist "ytsearch30:<query>"   (metadata only)
                               ◄─ results list (filtered, shuffled for random digs)
Listen       ─► Digger thread ─► yt-dlp -f bestaudio → ~/Library/Caches/CrateDig/Previews (temporary, pruned)
                               ─► decode → normalise → BPM / key
             ◄─ preview buffer swapped into the audio thread (try-lock, no allocation)
Download/Drag ─► 24-bit WAV of the chop → ~/Music/CrateDig (→ performExternalDragDropOfFiles)
```

yt-dlp is started directly with an argument list (no shell). Video ids are validated before use, and the search
text is passed as a single argument, so it cannot inject options.

| Path | Role |
|---|---|
| `Source/Core/YouTube.*` | Query building, result parsing/filtering, yt-dlp arguments (pure, unit-tested) |
| `Source/Core/YtDlp.*` | Locating and running yt-dlp / ffmpeg / deno |
| `Source/Core/Digger.*` | Background search and preview fetching, decoding, analysis, cache |
| `Source/Core/Analysis.*` | Onset-autocorrelation BPM, Krumhansl–Schmuckler key |
| `Source/Core/Exporter.*` | File naming and WAV export |
| `Source/PluginProcessor.*` | Search → select → listen → download flow, preview playback, state |
| `Source/PluginEditor.*`, `Source/UI/*` | Resizable UI, waveform/chop view, drag tile |

## Rights and YouTube's terms

Downloading from YouTube may be against [YouTube's Terms of Service](https://www.youtube.com/t/terms), and most
recordings are protected by copyright. CrateDig is a research and crate-digging tool: you choose what to fetch and
keep, and you are responsible for clearing any sample before releasing music that uses it.
CrateDig is not affiliated with samplette.io, YouTube or the yt-dlp project.

## Licence

[AGPL-3.0](LICENSE). Built with [JUCE](https://juce.com) (AGPLv3).

---

### 🇫🇷 En bref

CrateDig est un plugin AU / VST3 inspiré de samplette.io qui utilise **YouTube comme moteur de recherche**.
**DIG** lance une recherche aléatoire (genre / décennie), ou tape ta propre recherche. **Rien n'est téléchargé
automatiquement** : **Listen** récupère une pré-écoute temporaire, et **Download** (ou le glisser-déposer vers le DAW)
enregistre le WAV 24 bits dans `~/Music/CrateDig`. Le plugin demande **yt-dlp** (`brew install yt-dlp`).
La fenêtre est redimensionnable (coin en bas à droite ou boutons S / M / L).
