#pragma once

#include <filesystem>

namespace rex::ui {
class Window;
}

namespace hells_gate::ps5 {

// Returns the game-data folder when the player selects Play, or empty on Exit.
std::filesystem::path RunLauncher(rex::ui::Window* window,
                                  const std::filesystem::path& configured_game);

}  // namespace hells_gate::ps5
