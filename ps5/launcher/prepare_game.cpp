// SPDX-License-Identifier: GPL-3.0-or-later
#include "prepare_game.h"

#include <xxhash.h>

#include <cstdio>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" void title_note(const char* format, ...) __attribute__((format(printf, 1, 2)));

namespace hells_gate::ps5 {
namespace {

namespace fs = std::filesystem;

std::string Hex32(uint32_t value) {
  char text[9];
  std::snprintf(text, sizeof(text), "%08X", value);
  return text;
}

// Writes `data` to `path` through a temporary file, so a failure never leaves
// a partial file under the final name.
void WriteFileAtomically(const fs::path& path, const char* data, size_t size) {
  fs::path temporary = path;
  temporary += ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out.write(data, std::streamsize(size));
    out.close();
    if (!out) throw std::runtime_error("Could not write to the app folder (storage full?).");
  }
  std::error_code error;
  fs::rename(temporary, path, error);
  if (error) {
    fs::remove(temporary, error);
    throw std::runtime_error("Could not write to the app folder.");
  }
}

}  // namespace

bool gameReady(const fs::path& root) {
  std::error_code error;
  return !root.empty() && fs::is_regular_file(root / "default.xex", error) &&
         fs::is_regular_file(root / "default.xexp", error);
}

std::optional<fs::path> discGameReady(const fs::path& boot) {
  std::ifstream note(boot / "disc.txt");
  std::string iso, size_text;
  if (!std::getline(note, iso) || !std::getline(note, size_text) || iso.empty()) return std::nullopt;
  std::error_code error;
  const auto size = fs::file_size(iso, error);
  if (error || std::to_string(size) != size_text) return std::nullopt;
  if (!fs::is_regular_file(boot / "default.xexp", error)) return std::nullopt;
  return fs::path(iso);
}

fs::path prepareDiscGame(const PrepareConfig& config, const fs::path& boot) {
  const auto isos = findGameIsos(config.iso_roots);
  if (isos.empty()) {
    throw std::runtime_error("Place your Dante's Inferno ISO in the app's dump folder.");
  }

  // The disc is checked before the title update, so the update's messages can
  // assume the disc is the supported release.
  std::optional<fs::path> disc;
  std::string rejection = "No compatible Dante's Inferno ISO found.";
  for (const auto& iso : isos) {
    try {
      inspectGameIso(iso, [&config](std::span<const uint8_t> xex) {
        if (XXH64(xex.data(), xex.size(), 0) == config.base_xex_hash) return;
        const auto media = xexMediaId(xex);
        if (media && *media != config.update.media_id) {
          throw std::runtime_error("This disc is media ID " + Hex32(*media) +
                                   "; the port needs media ID " +
                                   Hex32(config.update.media_id) + ".");
        }
        throw std::runtime_error("This disc does not match the supported Dante's Inferno build.");
      });
      disc = iso;
      break;
    } catch (const std::exception& error) {
      title_note("launcher: ISO %s rejected: %s", iso.filename().c_str(), error.what());
      rejection = error.what();
    }
  }
  if (!disc) {
    throw std::runtime_error(isos.size() == 1 ? rejection
                                              : "No compatible Dante's Inferno ISO found.");
  }

  const std::vector<char> update = loadTitleUpdate(config.update_roots, config.update);

  std::error_code error;
  fs::create_directories(boot, error);
  if (!fs::is_directory(boot, error)) throw std::runtime_error("Could not create the app's discboot folder.");
  WriteFileAtomically(boot / "default.xexp", update.data(), update.size());
  const auto iso = fs::absolute(*disc, error);
  const std::string note = (error ? disc->string() : iso.string()) + "\n" +
                           std::to_string(fs::file_size(*disc)) + "\n";
  WriteFileAtomically(boot / "disc.txt", note.data(), note.size());
  title_note("launcher: prepared %s with the title update in %s", disc->filename().c_str(),
             boot.c_str());
  return error ? *disc : iso;
}

}  // namespace hells_gate::ps5
