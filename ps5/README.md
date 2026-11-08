# Hell's Gate PS5 bring-up

This branch adds a native Prospero homebrew target for Dante's Inferno. The first
boot path uses ReXGlue's statically linked Xenos Vulkan backend and the PS5
Vulkan stack from the existing PS5 ReXGlue template. Hell's Gate's DiligentCore
renderer, launcher, DLC installer and desktop overlays are outside this first
boot target.

The smoke app uses the same PS5 runtime and graphics libraries. It opens a
window, reads back a sentinel from the Xbox 360 guest heap, and initializes the
Xenos/RADV presenter without Xbox 360 game files.

## Build on Linux or WSL

Requirements are CMake, Ninja, Clang 21, Python 3, and the checked-out PS5
stack. For the host CLI, install the normal ReXGlue Linux build dependencies,
including Wayland and the Xlib/XCB development headers (Ubuntu
`libwayland-dev` and `libx11-xcb-dev`). Set the following from the repository
root, adjusting the paths to your checkouts.

```bash
export PS5_REXGLUE_TEMPLATE="$HOME/PS5_ReXGlueTemplate"
export PS5_VULKAN_DIR="$HOME/PS5_Vulkan"
export PS5_PAYLOAD_SDK="$PS5_REXGLUE_TEMPLATE/.deps/native/ps5-payload-sdk"
export REXGLUE_FORK="$HOME/rexglue-sdk"
export HELLS_GATE_BUILD_DIR=/var/tmp/hells-gate-build

bash ps5/build.sh runtime
bash ps5/build.sh host
bash ps5/build.sh smoke
```

The ReXGlue fork needs commit `f47ccdde34b3a992f7872a27619fc588040c229f`.
The build applies the Hell's Gate patch rebased onto that PS5 fork in an isolated
checkout. The host CLI and PS5 runtime come from that same patched source. The
template is pinned to `c91ff84b7f2cbe3884670fceed8dc924724e04cb`. Use its payload
SDK pin with the PS5 platform layer, plus a prepared PS5_Vulkan checkout with
RADV, the signing tool and libc.prx. Its sibling PS5_PayloadSDK and PS5_Mesa
checkouts are used to stage dependency notices. A local checkout can supply the
ReXGlue fork when its GitHub remote is inaccessible.

The package is under `$HELLS_GATE_BUILD_DIR/title-template/dist/PPSA99342/`.
The smoke and game builds share this development title ID. To run the smoke
test, register the title folder in your console's homebrew launcher and launch
**Hell's Gate (PS5 Development)**. Read `hells-gate-results.txt` from the title
folder. The final check is `smoke: PASS`.

## Resume this build on the current PC

The compiled tools and PS5 archives are in Ubuntu WSL under
`/var/tmp/hells-gate-ps5.SQu6Vr`. To reuse them, run the exports above and then
override the build paths in the same shell.

```bash
export HELLS_GATE_BUILD_DIR=/var/tmp/hells-gate-ps5.SQu6Vr
export REXGLUE_PS5_BUILD="$HELLS_GATE_BUILD_DIR/ps5-build"
```

The ISO was extracted into the WSL build directory's `disc/` folder.
The corresponding source checkout with the generated guest sources is under
`hells-gate-game/`. The build used the Title Update 2 patch from the upstream
portable release. The signed game build and game data are already staged at
`/data/homebrew/PPSA99342`. Register the folder in the homebrew launcher before
starting **Hell's Gate (PS5 Development)**.

## Build the game when the disc and update are ready

1. Extract your Xbox 360 disc into the repo's `game/` folder with the original
   directory layout. Place the matching Title Update 2 patch beside the
   executable as `game/default.xexp`. The manifest and guest hooks target TU2.
2. Run `bash ps5/build.sh codegen`, then `bash ps5/build.sh game`. Codegen runs
   on the host, followed by the upstream generated-code patch script. Inspect
   warnings about any remaining unresolved branches before deployment.
3. Deploy `PPSA99342/` and copy your extracted game files to the title's
   `gamedata/` directory. Keep `default.xex` and `default.xexp` together
   there. The first game boot should confirm TU2, initialize the guest and
   reach the first rendered frame.

## Verification so far

- Built the patched Linux ReXGlue CLI and cross-compiled `rexruntime` and
  `rexgpu-xenos` for PS5.
- Recompiled the extracted disc against Title Update 2. The generated-code
  patcher registered 36,983 functions and reported zero unresolved branch
  traps after applying its patches.
- Cross-compiled, linked and signed the game. Verified all five loadable ELF
  segments against the console copy. Verified sizes of all six uploaded data
  files and SHA-256 of both the XEX and XEXP.
- The console launcher reported title-not-found (`0x80940031`) before the
  executable ran. First boot, rendering, audio, input, saves and gameplay
  remain unverified.

References [Hell's Gate](https://github.com/florinp93/hells-gate-recomp),
[KongaTime's Edge of Time PS5 port](https://github.com/KongaTime/Spider-Man-Edge-Of-Time-PS5),
[PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk).
