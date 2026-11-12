<p align="center">
  <img src="ps5/docs/ps5-icon.png" alt="Dante’s Inferno PS5 home-screen icon" width="192">
</p>

# Dante’s Inferno PS5 Port

Play Dante’s Inferno on a homebrew-enabled PS5. The port includes a native renderer, PlayStation button prompts, and a launcher that prepares game data from your own Xbox 360 disc image.

## Install V1.2.0

1. Download `Dantes.Inferno.PS5.Port.V1.2.0.zip` from [GitHub Releases](https://github.com/KongaTime/Dantes-Inferno-PS5-Port/releases/tag/v1.2.0).
2. Extract the `PPSA99343` folder to `/data/homebrew/` on a PS5 set up to run homebrew. Register the title with ShadowMount+.
3. Place your Xbox 360 disc ISO in `PPSA99343/dump/` and the matching raw Title Update 2 in `PPSA99343/tu/default.xexp`. Create the folders if needed.
4. Open **Dantes Inferno** on the PS5 and choose **Prepare Game**. Once preparation finishes, choose **Play Game**.

If your game files are already extracted, put them in `PPSA99343/gamedata/` with both `default.xex` and `default.xexp` at that folder’s root. The download requires your own game data and title update. The port supports the USA/Europe disc (EA-2255, version 1.0) with Title Update 2.

To update from an earlier version, close the app and copy the new `PPSA99343` folder over the existing one. Your `gamedata/`, saves and settings stay in place.

## DLC (optional, new in V1.2.0)

The port can install your own Dante’s Inferno DLC: Trials of St. Lucia, the Dark Forest Pack, the costume packs, and the soul and relic packs.

1. Create a `dlc` folder inside `PPSA99343`.
2. Copy your DLC packages into it. These are the STFS files from an Xbox 360’s `Content` folder, usually named with a 42-character ID. You can copy the whole `454108CF` folder as it is, so the packages end up in `PPSA99343/dlc/454108CF/00000002/`. Pre-extracted content folders copied from Xenia also work.
3. Open the game and choose **Play Game**. The launcher shows **INSTALLING DLC** with each pack’s name and a progress bar. The game starts when it finishes.

Installing only happens once per package. Trials of St. Lucia is the largest pack, so the first launch can take several minutes; don’t close the app while it installs. Installed packages are listed in `dlc/.installed`. Delete a line from that file to install that package again. Files that aren’t Dante’s Inferno DLC, such as Xbox avatar items, are skipped, and the game runs normally without a `dlc` folder.

## Resolution

The default internal resolution is **1440p (2560×1440)**. To use **4K (3840×2160)**, select **Resolution** in the Recomp launcher, press Cross or D-pad Left/Right to change it, then exit and reopen the app. The launcher remembers your choice.

The native renderer, launcher-to-game handoff, and PlayStation prompts were tested on PS5 with prepared game files. DLC installation was tested on PS5 with all 13 DLC packages. ISO extraction passed host tests using a repacked image of the game files and the matching update. First-run ISO extraction on PS5 remains unverified.

## Build and source

See the [PS5 build notes](ps5/README.md) and the build-source archive on the [V1.2.0 release page](https://github.com/KongaTime/Dantes-Inferno-PS5-Port/releases/tag/v1.2.0).

## Credits

- **KongaTime** for the PS5 port and console testing.
- **florinp93 and the Hell’s Gate contributors** for the Xbox 360-to-PC recompilation this port builds on, including the original native renderer. [Hell’s Gate](https://github.com/florinp93/hells-gate-recomp)
- **The ReXGlue contributors** for the Xbox 360 recompilation tools and runtime. [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)
- **mihawk-99 and the Mesa/RADV contributors** for the Vulkan stack that runs the renderer on PS5. [PS5 Vulkan](https://github.com/mihawk-99/PS5_Vulkan)
- **The DiligentCore contributors** for the graphics framework used by the native renderer. [DiligentCore](https://github.com/DiligentGraphics/DiligentCore)
- **Kenney** for the controller icon assets used in the PlayStation prompts. [Icon license](assets/glyphs/kenney/LICENSE.txt)
