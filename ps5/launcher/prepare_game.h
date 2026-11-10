// Prepare Game: the disc image in dump/ plus the matching Title Update become
// gamedata/. Kept apart from the menu so the import can run on the host against
// a generated ISO, without a console or the retail disc.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "iso_import.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace hells_gate::ps5 {

// A prepared install: the executable and its title update side by side.
bool gameReady(const std::filesystem::path& root);

struct PrepareConfig {
  // Searched in order for the disc image.
  std::vector<std::filesystem::path> iso_roots;
  // The title update is read from `update`, else from `update_fallback`.
  std::filesystem::path update;
  std::filesystem::path update_fallback;
  // The build this launcher supports: XXH64 of default.xex and of the title
  // update, and the update's exact size.
  uint64_t base_xex_hash = 0;
  uint64_t update_hash = 0;
  uint64_t update_size = 0;
};

// Extracts the disc into `destination` and installs the title update beside the
// executable. Throws std::runtime_error with a message meant for the player, and
// leaves an existing install untouched when it fails.
void prepareGame(const PrepareConfig& config, const std::filesystem::path& destination,
                 const ImportProgress& progress = {});

}  // namespace hells_gate::ps5
