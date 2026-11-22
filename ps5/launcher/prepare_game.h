// Prepare Game: the disc image in dump/ is checked and played in place. Only
// the matching Title Update is written out, beside a note naming the image, so
// preparing takes seconds. (PS5 titles may write about 1.5 GB at full speed
// before writes drop to about 2 MB/s, which made copying the disc's 5.6 GB take
// most of an hour.) Kept apart from the menu so it can run on the host.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "iso_import.h"
#include "title_update.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace hells_gate::ps5 {

// An extracted install: the executable and its title update side by side.
bool gameReady(const std::filesystem::path& root);

struct PrepareConfig {
  // Searched in order for the disc image.
  std::vector<std::filesystem::path> iso_roots;
  // Files or folders searched in order for the title update: the extracted
  // default.xexp or the Title Update package as downloaded.
  std::vector<std::filesystem::path> update_roots;
  // The build this launcher supports: XXH64 of default.xex, and the title
  // update it needs.
  uint64_t base_xex_hash = 0;
  TitleUpdateSpec update;
};

// The disc image a previous Prepare recorded in `boot`, if it is still there
// at the same size and the title update beside the note is present.
std::optional<std::filesystem::path> discGameReady(const std::filesystem::path& boot);

// Finds and verifies the disc image and the title update, then writes the
// update to `boot`/default.xexp and the image's path to `boot`/disc.txt.
// Returns the image. Throws std::runtime_error with a message meant for the
// player, leaving an earlier preparation untouched.
std::filesystem::path prepareDiscGame(const PrepareConfig& config,
                                      const std::filesystem::path& boot);

}  // namespace hells_gate::ps5
