# Dante’s Inferno PS5 Port

A PS5 homebrew port of Hell’s Gate, the Xbox 360 recompilation of Dante’s Inferno. This release uses the native renderer and PlayStation button prompts.

## Install V1.0.0

1. Download `Dantes Infero PS5 Port V1.0.0.zip` from [GitHub Releases](https://github.com/KongaTime/Dantes-Inferno-PS5-Port/releases/tag/v1.0.0).
2. Extract the `PPSA99343` folder to `/data/homebrew/` on a PS5 set up to run homebrew. Register the title with ShadowMount+.
3. Place your Xbox 360 disc ISO in `PPSA99343/dump/` and the matching raw Title Update 2 in `PPSA99343/tu/default.xexp`. Create the folders if needed.
4. Open **Dantes Inferno** on the PS5 and choose **Prepare Game**. Once preparation finishes, choose **Play Game**.

If your game files are already extracted, put them in `PPSA99343/gamedata/` with both `default.xex` and `default.xexp` at that folder’s root. The download requires your own game data and title update.

The native renderer, launcher-to-game handoff, and PlayStation prompts were tested on PS5 with prepared game files. ISO extraction passed host tests using a repacked image of the game files and the matching update. First-run ISO extraction on PS5 remains unverified.

See the [PS5 build notes](ps5/README.md) for source and build details. The upstream Hell’s Gate documentation follows.

---
