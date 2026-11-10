#pragma once

#include <cstdint>
#include <filesystem>

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
std::filesystem::path RunLauncher(rex::ui::Window* window,
                                  const std::filesystem::path& configured_game,
                                  const RenderMode& mode);

}  // namespace hells_gate::ps5
