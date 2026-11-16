# AGENTS.md

Instructions for AI agents and contributors working on **Pokémon Fulldex**
(`mamesj/pokemon-fulldex`). Read this before building or changing anything.

## What this repository is

A C++17/SDL2 emulator front end: melonDS (DS games) and mGBA (GBA games, under `gba/`) with
a widescreen renderer, a launcher (`frontend/pc_launcher.cpp`), and Android/iOS/Switch/macOS
builds. The player supplies their own ROMs; **no ROMs, saves, keys or Nintendo artwork are
committed**.

## Build and test commands

### Recompiled native executable (single game, `setup.sh`)

The root `setup.sh` is the original ssrecomp path: it fetches the pokeheartgold decompilation,
builds the game from your own ROM, generates the recompiled C++ and compiles a **native
executable for that one ROM**. It is not the all-games launcher.

```bash
./setup.sh                 # asks before installing packages
./setup.sh --deps          # install packages without asking (sudo)
./setup.sh --decomp DIR    # use an existing pokeheartgold checkout
./setup.sh --rom FILE      # also check a ROM against the build
./setup.sh --jobs N
./setup.sh --desktop       # add a menu entry
```

The multi-game PC/Android builds below are different: they read the SoulSilver ROM only for
table addresses and do **not** contain game code.

### DS / PC all-games (Windows, Linux, Linux ARM64, AppImage)

```bash
tools/pc/build_all.sh windows|linux|linux-arm64|both "<path-to-soulsilver-us.nds>"
```

Outputs in `build-pc-dist/`. Needs Linux/WSL2 (Ubuntu 24.04), CMake, Ninja, Python 3, curl,
SDL2 dev headers, and MinGW-w64 for Windows.

Optional flags:
- `SSR_PC_ART=<folder>` — game-list pictures (e.g. `IREO.jpg`, `IRBO.jpg`)
- `SSR_PC_ICON=<file.ico>` — Windows icon
- `SSR_LINUX_ICON=<file>` — AppImage icon
- `SSR_GBA_DIR=<path-to-gba/>` — build GBA games in too

### Android

```bash
ANDROID_SDK=/opt/android-sdk tools/android/build_all.sh "<path-to-soulsilver-us.nds>"
```

Output: `build-android-dist/pokemon-ds-wide-android.apk`. Requires SDK build-tools 34.0.0,
platform android-34, NDK 27.2.12479018, JDK 17, Pillow.

Optional: `SSR_ANDROID_KEY`, `SSR_ANDROID_ICON`, `SSR_PC_ART`, `SSR_GBA_DIR`.

### GBA support

The GBA player is `gba/` (mGBA + wide renderer + the player frontend). To build it in:

1. Prepare the pinned mGBA and patches (`tools/apple/fetch_gba.sh` does this for Apple builds;
   the same steps work anywhere):

   ```bash
   SRC="$PWD/gba"; WORK="$HOME/pokemon-gba-wide"
   git init -q "$WORK/mgba"
   git -C "$WORK/mgba" remote add origin https://github.com/mgba-emu/mgba.git
   git -C "$WORK/mgba" fetch -q --depth 1 origin "$(tr -d '[:space:]' < "$SRC/patches/mgba.commit")"
   git -C "$WORK/mgba" checkout -q --force FETCH_HEAD
   git -C "$WORK/mgba" apply "$SRC/patches/mgba-wide.patch"
   mkdir -p "$WORK/build/patches"
   cp "$SRC"/wide-patches/*.ips "$SRC"/wide-patches/*.safety "$WORK/build/patches"/
   ```

2. Build with `SSR_GBA_DIR=$PWD/gba`. The CMake expects the work folder at
   `$HOME/pokemon-gba-wide` (or set `GBAW_WORK`/`GBAW_MGBA`/`GBAW_PATCHES`).

**Gotchas**
- In containers, create a symlink inside the container:
  `ln -sfn /gba-work /root/pokemon-gba-wide` (CMake uses `$HOME/pokemon-gba-wide`).
- Pass `SSR_GBA_DIR`, `GBAW_WORK`, `GBAW_MGBA`, `GBAW_PATCHES` into the container; an empty
  `SSR_GBA_DIR` silently builds a DS-only APK.
- The Android APK containing GBA is larger (`LICENSE-mGBA.txt` present). Verify with
  `unzip -l ... | grep LICENSE-mGBA`.

### Box art

- **DS:** `SSR_PC_ART=<folder>` with `<code>.jpg` (IREO, IRBO, IPGE, …).
- **GBA:** `gba/art/<name>.jpg` (512×512), names from `gba/frontend/games.h` (`emerald`,
  `firered`, `leafgreen`, `ruby`, `sapphire`, `ultraviolet`, `radicalred`, `unbound`,
  `emeraldlegacy`, `monomyth`, …). `gba/tools/embed_art.py` embeds them.
- **The GBA art is not in the repo** (Nintendo artwork, see `gba/README.md`). To test with it,
  extract from a release APK (`unzip`, then carve 512×512 JPEGs from `lib/arm64-v8a/libmain.so`)
  or supply your own; **do not commit it** — `gba/art/` is gitignored.

### Tests

```bash
python3 -I -m unittest discover -s tests -p 'test_*.py'
```

- Some tests need `cc`; a container without a C compiler will fail `test_gba_wide_clip`
  (environment issue, not a code failure).
- Tests under `tests/` include the VDF/Steam writer, cloud saves, launcher input and the
  Android native tests.

## Repository rules

- **Never commit:** ROMs, saves, save states, the Google sign-in client
  (`fulldex-google-client.txt`, `~/.config/pokemon-ds-wide/fulldex-google-client.txt`),
  signing keys, or Nintendo artwork (`gba/art/`). `.gitignore` covers these; check
  `git status --short` before committing.
- Work on a branch, not `main`. Use the existing issue templates
  (`.github/ISSUE_TEMPLATE/`) for bugs/features.
- Keep commits focused; separate unrelated fixes.
- Match the existing style: the codebase favours small, direct changes, short comments in
  parentheses explaining *why*, and no unnecessary abstraction.
- The launcher input model: pointer/touch hit-tests the target and focuses/activates it;
  keyboard/gamepad navigate focus and activate with A/Enter. Keep both paths in sync
  (see `frontend/pc_launcher.cpp`, `choose()` and the `FINGER`/`MOUSE` handlers in
  `frontend/main.cpp`).
- Steam shortcuts (`frontend/steam_shortcuts.h`, `frontend/vdf.h`) preserve the user's
  `shortcuts.vdf` byte-for-byte apart from the target entry; do not re-encode the whole file.
- Build artifacts/binaries live in `build-*/` and are ignored.

## Environment notes

- The repository may be on a root-owned mount; use `git -c safe.directory=$PWD ...`.
- On SteamOS/immutable systems, build in a container (Ubuntu 24.04) rather than installing
  system packages.
- Android test builds are signed with a per-build test key; installing over an official APK
  requires uninstalling it (signature mismatch) and restoring app data afterwards.

## Useful references

- `README.md` — player-facing features and installs.
- `tools/pc/README.txt` — the PC/Steam Deck download instructions.
- `gba/README.md` — the GBA player and its licensing.
- `CHANGELOG.md` — release history.
- `.github/ISSUE_TEMPLATE/` — bug/feature forms.
