// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_update.h"

#include <xxhash.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace hells_gate::ps5 {
namespace {

namespace fs = std::filesystem;

constexpr uint64_t kMaxCandidateSize = 64ull << 20;
constexpr size_t kMaxCandidates = 64;
constexpr uint32_t kBlockSize = 0x1000;
constexpr uint32_t kBlocksPerHashLevel[3] = {170, 170 * 170, 170 * 170 * 170};
constexpr uint32_t kEndOfChain = 0xFFFFFF;
constexpr uint32_t kTitleUpdateContent = 0x000B0000;

void require(bool ok, const char* text) {
  if (!ok) throw std::runtime_error(text);
}

uint32_t be32(const uint8_t* b) {
  return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
}
uint16_t le16(const uint8_t* b) { return uint16_t(b[0] | b[1] << 8); }
uint32_t le24(const uint8_t* b) { return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16; }

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  }
  return out;
}

std::string hex32(uint32_t value) {
  char text[9];
  std::snprintf(text, sizeof(text), "%08X", value);
  return text;
}

// Block addressing for STFS volumes, as in ReXGlue's StfsContainerDevice.
class Stfs {
 public:
  explicit Stfs(std::span<const uint8_t> data) : data_(data) {
    require(data.size() >= 0x3AD, "The title update package is truncated.");
    const uint32_t header_size = be32(&data[0x340]);
    base_ = (uint64_t(header_size) + kBlockSize - 1) & ~uint64_t(kBlockSize - 1);
    require(data[0x379] == 0x24 && be32(&data[0x3A9]) == 0,
            "The title update package uses an unsupported layout.");
    const uint8_t flags = data[0x37B];
    read_only_ = flags & 1;
    root_active_ = flags & 2;
    table_count_ = le16(&data[0x37C]);
    table_block_ = le24(&data[0x37E]);
    total_blocks_ = be32(&data[0x395]);
    per_table_ = read_only_ ? 1 : 2;
    step_[0] = kBlocksPerHashLevel[0] + per_table_;
    step_[1] = kBlocksPerHashLevel[1] + (kBlocksPerHashLevel[0] + 1) * per_table_;
  }

  uint32_t table_block() const { return table_block_; }
  uint32_t table_count() const { return table_count_; }

  std::span<const uint8_t> block(uint32_t index) const {
    require(index < kEndOfChain, "The title update package has a broken block chain.");
    return at(dataOffset(index), kBlockSize);
  }

  uint32_t next(uint32_t index) const {
    uint64_t secondary = (!read_only_ && root_active_) ? kBlockSize : 0;
    if (!read_only_ && total_blocks_ > kBlocksPerHashLevel[0]) {
      if (total_blocks_ > kBlocksPerHashLevel[1]) {
        const uint32_t record = (index / kBlocksPerHashLevel[1]) % kBlocksPerHashLevel[0];
        secondary = active(hashOffset(index, 2) + secondary, record) ? kBlockSize : 0;
      }
      const uint32_t record = (index / kBlocksPerHashLevel[0]) % kBlocksPerHashLevel[0];
      secondary = active(hashOffset(index, 1) + secondary, record) ? kBlockSize : 0;
    }
    const auto entry = at(hashOffset(index, 0) + secondary + (index % kBlocksPerHashLevel[0]) * 0x18, 0x18);
    return be32(&entry[0x14]) & 0xFFFFFF;
  }

 private:
  std::span<const uint8_t> at(uint64_t offset, uint64_t size) const {
    require(offset <= data_.size() && size <= data_.size() - offset,
            "The title update package is truncated.");
    return data_.subspan(size_t(offset), size_t(size));
  }

  bool active(uint64_t table, uint32_t record) const {
    return be32(&at(table + record * 0x18, 0x18)[0x14]) & 0x40000000;
  }

  uint64_t dataOffset(uint64_t index) const {
    uint64_t base = kBlocksPerHashLevel[0];
    uint64_t block = index;
    for (int level = 0; level < 3; ++level) {
      block += ((index + base) / base) * per_table_;
      if (index < base) break;
      base *= kBlocksPerHashLevel[0];
    }
    return base_ + (block << 12);
  }

  uint64_t hashOffset(uint32_t index, int level) const {
    uint64_t block = 0;
    if (level == 0) {
      if (index >= kBlocksPerHashLevel[0]) {
        block = uint64_t(index / kBlocksPerHashLevel[0]) * step_[0];
        block += uint64_t(index / kBlocksPerHashLevel[1] + 1) * per_table_;
        if (index >= kBlocksPerHashLevel[1]) block += per_table_;
      }
    } else if (level == 1) {
      block = index < kBlocksPerHashLevel[1]
                  ? step_[0]
                  : uint64_t(index / kBlocksPerHashLevel[1]) * step_[1] + per_table_;
    } else {
      block = step_[1];
    }
    return base_ + (block << 12);
  }

  std::span<const uint8_t> data_;
  uint64_t base_ = 0;
  bool read_only_ = false;
  bool root_active_ = false;
  uint32_t table_count_ = 0;
  uint32_t table_block_ = 0;
  uint32_t total_blocks_ = 0;
  uint32_t per_table_ = 1;
  uint64_t step_[2] = {};
};

bool isPackage(std::span<const uint8_t> data) {
  if (data.size() < 4) return false;
  const std::string_view magic(reinterpret_cast<const char*>(data.data()), 4);
  return magic == "LIVE" || magic == "CON " || magic == "PIRS";
}

bool isXex(std::span<const uint8_t> data) {
  return data.size() >= 4 && std::string_view(reinterpret_cast<const char*>(data.data()), 4) == "XEX2";
}

bool matches(std::span<const uint8_t> xexp, const TitleUpdateSpec& spec) {
  if (xexp.size() != spec.size) return false;
  const uint64_t hash = XXH64(xexp.data(), xexp.size(), 0);
  return std::find(spec.hashes.begin(), spec.hashes.end(), hash) != spec.hashes.end();
}

// Every file under `roots` worth looking at, in a stable order.
std::vector<fs::path> candidates(const std::vector<fs::path>& roots) {
  std::vector<fs::path> found;
  auto add = [&found](const fs::path& path) {
    std::error_code error;
    if (found.size() < kMaxCandidates && fs::is_regular_file(fs::status(path, error)) &&
        std::find(found.begin(), found.end(), path) == found.end()) {
      found.push_back(path);
    }
  };
  for (const auto& root : roots) {
    std::error_code error;
    const auto status = fs::status(root, error);
    if (error) continue;
    if (fs::is_regular_file(status)) {
      add(root);
      continue;
    }
    if (!fs::is_directory(status)) continue;
    std::vector<fs::path> inside;
    for (fs::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
      if (it.depth() >= 1) it.disable_recursion_pending();
      if (inside.size() >= kMaxCandidates) break;
      inside.push_back(it->path());
    }
    std::sort(inside.begin(), inside.end());
    for (const auto& path : inside) add(path);
  }
  return found;
}

}  // namespace

std::optional<uint32_t> xexMediaId(std::span<const uint8_t> xex) {
  if (!isXex(xex) || xex.size() < 0x18) return std::nullopt;
  const uint32_t count = be32(&xex[0x14]);
  for (uint32_t i = 0; i < count && 0x20 + uint64_t(i) * 8 <= xex.size(); ++i) {
    const uint8_t* header = &xex[0x18 + i * 8];
    if (be32(header) != 0x00040006) continue;
    const uint32_t offset = be32(header + 4);
    if (uint64_t(offset) + 4 > xex.size()) return std::nullopt;
    return be32(&xex[offset]);
  }
  return std::nullopt;
}

std::optional<std::vector<char>> extractStfsFile(std::span<const uint8_t> package,
                                                 std::string_view name) {
  require(isPackage(package), "Not a title update package.");
  const Stfs stfs(package);
  const std::string wanted = lower(name);
  uint32_t table = stfs.table_block();
  for (uint32_t n = 0; n < stfs.table_count() && table != kEndOfChain; ++n) {
    const auto entries = stfs.block(table);
    for (size_t e = 0; e < kBlockSize; e += 0x40) {
      const uint8_t* entry = &entries[e];
      if (entry[0] == 0) break;
      const uint8_t flags = entry[0x28];
      const size_t length = std::min<size_t>(flags & 0x3F, 0x28);
      if ((flags & 0x80) ||
          lower(std::string_view(reinterpret_cast<const char*>(entry), length)) != wanted) {
        continue;
      }
      const bool contiguous = flags & 0x40;
      const uint32_t size = be32(entry + 0x34);
      require(size <= kMaxCandidateSize, "The title update package is too large.");
      std::vector<char> out;
      out.reserve(size);
      uint32_t index = le24(entry + 0x2F);
      while (out.size() < size) {
        const auto data = stfs.block(index);
        const size_t take = std::min<size_t>(kBlockSize, size - out.size());
        out.insert(out.end(), data.begin(), data.begin() + take);
        index = contiguous ? index + 1 : stfs.next(index);
      }
      return out;
    }
    table = stfs.next(table);
  }
  return std::nullopt;
}

std::vector<char> loadTitleUpdate(const std::vector<fs::path>& roots, const TitleUpdateSpec& spec) {
  const std::string want_media = hex32(spec.media_id);
  // The most useful explanation seen so far; a higher rank is closer to right.
  int best_rank = 0;
  std::string best = "Place Title Update 2 for media ID " + want_media + " in the tu folder.";
  auto note = [&](int rank, std::string message) {
    if (rank > best_rank) {
      best_rank = rank;
      best = std::move(message);
    }
  };

  for (const auto& path : candidates(roots)) {
    // The menu wraps its status line at spaces; downloaded packages often have
    // long unbroken names, so those are not shown.
    std::string file = path.filename().string();
    if (file.size() > 24) file = "A file in the tu folder";
    std::error_code error;
    const uint64_t size = fs::file_size(path, error);
    if (error || size < 4 || size > kMaxCandidateSize) {
      note(1, file + " is not a title update.");
      continue;
    }
    std::vector<uint8_t> data(size);
    std::ifstream in(path, std::ios::binary);
    if (!in.read(reinterpret_cast<char*>(data.data()), std::streamsize(size))) {
      note(1, "Could not read " + file + ".");
      continue;
    }

    if (isXex(data)) {
      if (matches(data, spec)) return {data.begin(), data.end()};
      note(4, file + " is not Title Update 2 for media ID " + want_media + ".");
      continue;
    }
    if (!isPackage(data)) {
      note(1, file + " is not a title update.");
      continue;
    }

    const uint32_t content = be32(&data[0x344]);
    const uint32_t media = be32(&data[0x354]);
    const uint32_t title = be32(&data[0x360]);
    if (title != spec.title_id || content != kTitleUpdateContent) {
      note(2, file + " is not a Dante's Inferno title update.");
      continue;
    }
    if (media != spec.media_id) {
      note(3, file + " is for disc media ID " + hex32(media) + "; this port needs the update for " +
                  want_media + ".");
      continue;
    }
    try {
      const auto xexp = extractStfsFile(data, "default.xexp");
      if (!xexp) {
        note(4, file + " has no default.xexp inside.");
      } else if (matches(std::span(reinterpret_cast<const uint8_t*>(xexp->data()), xexp->size()), spec)) {
        return *xexp;
      } else {
        note(4, file + " is a different title update; this port needs Title Update 2.");
      }
    } catch (const std::exception& failure) {
      note(4, file + ": " + failure.what());
    }
  }
  throw std::runtime_error(best);
}

}  // namespace hells_gate::ps5
