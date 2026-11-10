// SPDX-License-Identifier: GPL-3.0-or-later
#include "prepare_game.h"

#include <xxhash.h>

#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" void title_note(const char* format, ...) __attribute__((format(printf, 1, 2)));

namespace hells_gate::ps5 {
namespace {

namespace fs = std::filesystem;

fs::path TitleUpdatePath(const PrepareConfig& config) {
  std::error_code error;
  if (!config.update.empty() && fs::is_regular_file(config.update, error)) return config.update;
  return config.update_fallback;
}

std::vector<char> ReadTitleUpdate(const PrepareConfig& config) {
  const fs::path path = TitleUpdatePath(config);
  std::error_code error;
  if (fs::file_size(path, error) != config.update_size || error) {
    throw std::runtime_error("Place the matching Title Update 2 in tu/default.xexp.");
  }
  std::vector<char> data(config.update_size);
  std::ifstream file(path, std::ios::binary);
  if (!file.read(data.data(), std::streamsize(data.size())) ||
      XXH64(data.data(), data.size(), 0) != config.update_hash) {
    throw std::runtime_error("Title Update 2 does not match this build.");
  }
  return data;
}

}  // namespace

bool gameReady(const fs::path& root) {
  std::error_code error;
  return !root.empty() && fs::is_regular_file(root / "default.xex", error) &&
         fs::is_regular_file(root / "default.xexp", error);
}

void prepareGame(const PrepareConfig& config, const fs::path& destination,
                 const ImportProgress& progress) {
  const auto isos = findGameIsos(config.iso_roots);
  if (isos.empty()) {
    throw std::runtime_error("Place your Dante's Inferno ISO in the app's dump folder.");
  }
  const std::vector<char> update = ReadTitleUpdate(config);

  std::optional<IsoPlan> plan;
  for (const auto& iso : isos) {
    try {
      plan = inspectGameIso(iso, [&config](std::span<const uint8_t> xex) {
        if (XXH64(xex.data(), xex.size(), 0) != config.base_xex_hash) {
          throw std::runtime_error("This disc does not match the supported Dante's Inferno build.");
        }
      });
      break;
    } catch (const std::exception& error) {
      title_note("launcher: ISO rejected: %s", error.what());
    }
  }
  if (!plan) throw std::runtime_error("No compatible Dante's Inferno ISO found.");
  extractGameIso(*plan, destination, progress, [&update](const fs::path& stage) {
    std::ofstream output(stage / "default.xexp", std::ios::binary);
    output.write(update.data(), std::streamsize(update.size()));
    output.close();
    if (!output) throw std::runtime_error("Could not install Title Update 2.");
  });
}

}  // namespace hells_gate::ps5
