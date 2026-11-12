#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace rex::ui {
class Window;
}

namespace hells_gate::ps5 {

struct RenderMode {
  uint32_t width;
  uint32_t height;
  const char* label;
  const char* resolution;
  const char* cache_directory;
};

inline constexpr RenderMode k1440p{2560, 1440, "1440p", "2560x1440", "1440p"};
inline constexpr RenderMode k4K{3840, 2160, "4K", "3840x2160", "2160p"};

// The swap chain is created once per launch, then handed to the game.
const RenderMode& LoadRenderMode();

// Returns the game-data folder when the player selects Play, or empty on Exit.
// After Play the launcher keeps the screen: call ShowLauncherStatus for any
// work done before the game starts, then HandOffLauncher before the game
// renderer starts.
std::filesystem::path RunLauncher(rex::ui::Window* window,
                                  const std::filesystem::path& configured_game,
                                  const RenderMode& mode);

// Draws one launcher frame with `title` as the action row, `status` below it
// and a progress bar at `progress` (0..1). Does nothing once handed off.
void ShowLauncherStatus(const std::string& title, const std::string& status, float progress);

// Gives the launcher's swap chain to the game renderer. Safe to call twice.
void HandOffLauncher();

}  // namespace hells_gate::ps5
