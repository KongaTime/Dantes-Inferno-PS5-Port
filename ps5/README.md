# Hell's Gate PS5 bring-up

This branch adds a native Prospero homebrew target for Dante's Inferno. The first
boot path uses ReXGlue's statically linked Xenos Vulkan backend and the PS5
Vulkan stack from the existing PS5 ReXGlue template. Hell's Gate's DiligentCore
renderer, launcher, DLC installer and desktop overlays are outside this first
boot target.

The smoke app uses the same PS5 runtime and graphics libraries. It opens a
window, reads back a sentinel from the Xbox 360 guest heap, and initializes the
Xenos/RADV presenter without Xbox 360 game files.

The separate native-renderer smoke title (`PPSA99343`) creates a DiligentCore
Vulkan device through RADV, verifies an offscreen clear by reading back a pixel,
then displays a green frame through PS5 VideoOut. It uses no game data.
After building the PS5 runtime, build it with `bash ps5/build.sh native-deps`
and `bash ps5/build.sh native-smoke`. The package is under
`native-title-template/dist/PPSA99343` inside the build directory.

With recompiled game sources and the host `glslangValidator` installed,
`bash ps5/build.sh native-game` builds the native game executable for
title PPSA99343. It uses DiligentCore for draw calls and a headless Xenos command
processor for guest GPU synchronization. Supply the game files in `gamedata/`
or set `game_data_root` in the test title's `dantes_inferno.toml`.

On the console, the launcher shows **Play Game** for a prepared installation.
For a fresh installation, place the supported Xbox 360 disc ISO in `dump/`
and the matching raw Title Update 2 in `tu/default.xexp` under the title folder.
Select **Prepare Game** to extract into `gamedata/`. A failed import keeps the
existing game folder intact. Press Cross to select or Circle to exit.

To use local launcher art, put `launcher.png` in
`ps5/native-smoke/title-art/` before building the native game.

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

## V1.0.0 source and dependencies

The release includes a build-source archive with the pinned ReXGlue and title
template snapshots, dependency diffs from the build, and the template licence.
Apply the dependency diffs to the listed base revisions to reproduce the
dependency source trees. The port uses the retail disc and matching TU2 at
build time to generate guest sources locally.

For the native release, run the `runtime`, `host`, `codegen`, `native-deps`,
and `native-game` modes in order. Use the host CLI path from the host build as
`REXGLUE_CODEGEN` and install `glslangValidator` before building the title.

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
- ShadowMount+ registered the title after `sce_sys/icon0.png` was added and
  its retry counter reset. On first boot the runtime applied TU2, created
  a VideoOut swapchain, started the guest, and submitted GPU and audio work.
  Visual output, controller response, saves and gameplay remain unverified.

References [Hell's Gate](https://github.com/florinp93/hells-gate-recomp),
[KongaTime's Edge of Time PS5 port](https://github.com/KongaTime/Spider-Man-Edge-Of-Time-PS5),
[PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk).
