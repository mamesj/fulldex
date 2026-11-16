# Developing Pokémon Fulldex

This is the developer guide for building, testing and packaging Fulldex. For the player-facing
guide see `README.md`; for AI-agent-specific rules see `AGENTS.md`.

## Layout

| Path | What it is |
| --- | --- |
| `frontend/` | C++17/SDL2 front end and the all-games launcher (`pc_launcher.cpp`) |
| `melonDS/` | DS emulator core |
| `gba/` | GBA player: mGBA + widescreen renderer + its frontend |
| `tools/pc/` | PC/Steam Deck build and packaging |
| `tools/android/` | Android build and packaging |
| `tools/apple/` | macOS/iOS helpers, including `fetch_gba.sh` |
| `tools/switch/` | Switch packaging |
| `tests/` | Python/C++/Android tests |
| `build-*/` | build output (ignored) |

## Prerequisites

Common: Linux or WSL2 (Ubuntu 24.04), Git, CMake, Ninja, Python 3, curl, SDL2 development
headers, libpng, zlib.

- Windows target: MinGW-w64 (`g++-mingw-w64-x86-64-posix`)
- Linux AppImage: `zsync`, `appimagetool` (fetched by the script)
- Linux ARM64: `g++-aarch64-linux-gnu`, `patchelf`
- Android: Android SDK with `build-tools;34.0.0`, `platforms;android-34`,
  `ndk;27.2.12479018`, JDK 17, `python3-pil`
- GBA: the patched mGBA and wide patches (see below)

On immutable systems (SteamOS, Bazzite, Silverblue) build inside a Ubuntu 24.04 container or
distrobox rather than modifying the host.

## Recompiled native executable (single game)

The root `setup.sh` is the original ssrecomp path: it fetches the pokeheartgold decompilation,
builds the game from your own ROM, generates the recompiled C++ and compiles a native
executable for that one ROM. It is **not** the all-games launcher.

```bash
./setup.sh                 # asks before installing packages
./setup.sh --deps          # install packages without asking (sudo)
./setup.sh --decomp DIR    # use an existing pokeheartgold checkout
./setup.sh --rom FILE      # also check a ROM against the build
./setup.sh --jobs N
./setup.sh --desktop       # add a menu entry
```

The multi-game PC/Android builds below are a different path: they read the SoulSilver ROM only
for table addresses and ship no game code.

## DS / PC build

```bash
tools/pc/build_all.sh linux        "<your SoulSilver US.nds>"
tools/pc/build_all.sh windows      "<your SoulSilver US.nds>"
tools/pc/build_all.sh linux-arm64  "<your SoulSilver US.nds>"
tools/pc/build_all.sh both         "<your SoulSilver US.nds>"
```

The SoulSilver ROM is used only to read table addresses; the downloads contain no game data.
Outputs land in `build-pc-dist/`:

- `pokemon-ds-wide-pc-windows-x86_64.zip`
- `pokemon-ds-wide-pc-linux-x86_64.tar.gz`
- `pokemon-ds-wide-pc-linux-x86_64.AppImage` (+ `.zsync`)
- `pokemon-ds-wide-pc-linux-aarch64.*`

Optional environment variables:

- `SSR_PC_ART=<folder>` — game-list box art, `<code>.jpg` (IREO, IRBO, IPGE …)
- `SSR_PC_ICON=<file.ico>` — Windows icon
- `SSR_LINUX_ICON=<file>` — AppImage icon
- `SSR_GBA_DIR=<path>` — include GBA games

## Android build

```bash
ANDROID_SDK=/opt/android-sdk tools/android/build_all.sh "<your SoulSilver US.nds>"
```

Output: `build-android-dist/pokemon-ds-wide-android.apk`.

Optional:

- `SSR_ANDROID_ABIS="arm64-v8a"` — phone-only
- `SSR_ANDROID_KEY=<key.json>` — signing key (default `~/.config/pokemon-ds-wide/…`); keep it,
  Android only installs updates signed with the same key
- `SSR_ANDROID_ICON=<png>` — launcher icon
- `SSR_PC_ART=<folder>` — game-list art
- `SSR_GBA_DIR=<path>` — include GBA

The first start asks for the ROM folder (uses Android's file access); saves live in a `saves`
folder inside it.

## GBA build

The GBA player source is `gba/` (mGBA with `patches/mgba-wide.patch`, the player frontend, and
`wide-patches/`). `gba/README.md` states:

> The release builds point `SSR_GBA_DIR` at this folder.
> Not included: the games' box art (`art/*.jpg`, Nintendo's artwork).

Prepare the work folder:

```bash
SRC="$PWD/gba"
WORK="$HOME/pokemon-gba-wide"
git init -q "$WORK/mgba"
git -C "$WORK/mgba" remote add origin https://github.com/mgba-emu/mgba.git
git -C "$WORK/mgba" fetch -q --depth 1 origin "$(tr -d '[:space:]' < "$SRC/patches/mgba.commit")"
git -C "$WORK/mgba" checkout -q --force FETCH_HEAD
git -C "$WORK/mgba" apply "$SRC/patches/mgba-wide.patch"
mkdir -p "$WORK/build/patches"
cp "$SRC"/wide-patches/*.ips "$SRC"/wide-patches/*.safety "$WORK/build/patches"/
```

Then build with `SSR_GBA_DIR="$SRC"`. The CMake checks:

- `$HOME/pokemon-gba-wide/mgba/include/mgba/internal/gba/video.h`
- `$HOME/pokemon-gba-wide/build/patches/<game>-wide.ips`

Override with `GBAW_WORK`, `GBAW_MGBA`, `GBAW_PATCHES`.

### Container gotcha

CMake uses `$ENV{HOME}/pokemon-gba-wide`. If you mount the work folder elsewhere (e.g.
`/gba-work`), symlink it inside the container:

```bash
ln -sfn /gba-work /root/pokemon-gba-wide
```

and pass `SSR_GBA_DIR`, `GBAW_WORK`, `GBAW_MGBA`, `GBAW_PATCHES`. An empty `SSR_GBA_DIR`
silently produces a DS-only APK.

## Box art

### DS

`SSR_PC_ART` folder, `<code>.jpg`, named as for the list's `icons/` folder (IREO, IRBO, IPGE,
IPKE, CPUE, AD…). CMake embeds them into the binary.

### GBA

`gba/art/<name>.jpg` (512×512), names from `gba/frontend/games.h`:

`emerald`, `emeraldlegacy`, `firered`, `leafgreen`, `monomyth`, `radicalred`, `ruby`,
`sapphire`, `ultraviolet`, `unbound`, plus regional/hack variants.

`gba/tools/embed_art.py` builds the header. The art is **not in the repository** (Nintendo
artwork) and `gba/art/` is gitignored. To test with it, extract the 512×512 JPEGs from a
release APK's `lib/arm64-v8a/libmain.so` or supply your own — never commit them.

## Tests

```bash
python3 -I -m unittest discover -s tests -p 'test_*.py'
```

Notes:

- Some C tests need `cc`; without a C compiler `test_gba_wide_clip` errors (environment, not
  code).
- Launcher input tests and the VDF/Steam writer tests live in `tests/`.
- Android tests are under `tests/android/`.

## Coding notes

- C++17, SDL2. Keep changes small and direct; comments in parentheses explain *why*.
- Launcher input: pointer/touch hit-tests the target, focuses it, and activates it; keyboard/
  gamepad navigate focus and activate with A/Enter. Keep `choose()`, the `SDL_FINGER*` handlers
  and the mouse handlers consistent.
- Steam `shortcuts.vdf` must stay byte-for-byte identical apart from the edited entry
  (`frontend/vdf.h`, `frontend/steam_shortcuts.h`).
- Do not commit ROMs, saves, keys, the Google client, or Nintendo artwork.

## Branches, issues and PRs

- Work on a branch (`fix/…`, `feature/…`); never push directly to `main`.
- File bug/feature issues with the templates in `.github/ISSUE_TEMPLATE/`.
- Keep commits focused and describe *why* in the body.
- The repository may be on a root-owned mount; use
  `git -c safe.directory=$PWD <command>`.

## Release packaging

The release assets are produced by the `tools/*/build_all.sh` scripts (PC, Android, Switch,
Apple). The Windows release is `pokemon-ds-wide-pc-windows-x86_64.zip`; the Android release is
`pokemon-ds-wide-android.apk`. Remember to include any required licences (`LICENSE-mGBA.txt`
when GBA is built in).
