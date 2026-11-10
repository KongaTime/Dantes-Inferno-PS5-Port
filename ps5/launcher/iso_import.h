// Adapted from KongaTime/The-Darkness-Recomp-PS5 app/ps5/iso_import.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace hells_gate::ps5 {
struct IsoEntry {
    std::filesystem::path path;
    uint64_t offset = 0, size = 0;
    bool directory = false;
};
struct IsoPlan {
    std::filesystem::path source;
    uint64_t sourceSize = 0, bytes = 0;
    uint64_t executableHash = 0;
    std::vector<IsoEntry> entries;
};
using ImportProgress = std::function<void(uint64_t, uint64_t, const std::string&)>;
using FinalizeImport = std::function<void(const std::filesystem::path&)>;
using VerifyExecutable = std::function<void(std::span<const uint8_t>)>;
// Inspection is read-only, bounded, and verifies default.xex before any extraction.
IsoPlan inspectGameIso(const std::filesystem::path&, const VerifyExecutable&);
// Extract beside destination, then rename only a complete tree into place.
// Never merges with or overwrites an existing nonempty destination.
void extractGameIso(const IsoPlan&, const std::filesystem::path&, const ImportProgress& = {},
                    const FinalizeImport& = {});
std::vector<std::filesystem::path> findGameIsos(const std::vector<std::filesystem::path>& roots);
}
