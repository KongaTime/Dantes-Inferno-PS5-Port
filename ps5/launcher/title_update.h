// Finds the title update the player supplied: either the extracted
// default.xexp or the Title Update package as downloaded (an STFS LIVE/CON/PIRS
// container), from which default.xexp is pulled out in memory.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace hells_gate::ps5 {

struct TitleUpdateSpec {
  // XXH64s of the accepted default.xexp builds, and their exact size.
  std::vector<uint64_t> hashes;
  uint64_t size = 0;
  // The game and the disc release the update must be for.
  uint32_t title_id = 0;
  uint32_t media_id = 0;
};

// Searches `roots` in order. A root that is a file is a candidate; a root that
// is a folder contributes the files inside it (two levels deep, so a folder
// named default.xexp or an extracted package folder still works). Returns the
// first candidate whose default.xexp matches `spec`, else throws
// std::runtime_error with a message for the player naming the closest miss.
std::vector<char> loadTitleUpdate(const std::vector<std::filesystem::path>& roots,
                                  const TitleUpdateSpec& spec);

// The bytes of the file called `name` (case-insensitive) inside an STFS
// package, or nothing when the package has no such file. Throws
// std::runtime_error when the package is malformed.
std::optional<std::vector<char>> extractStfsFile(std::span<const uint8_t> package,
                                                 std::string_view name);

// The media ID from an XEX2 executable's execution info, if present.
std::optional<uint32_t> xexMediaId(std::span<const uint8_t> xex);

}  // namespace hells_gate::ps5
