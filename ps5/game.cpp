#include "generated/default/dantes_inferno_init.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <rex/cvar.h>
#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/logging/macros.h>
#include <rex/memory/utils.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>
#include <rex/system/xmemory.h>

#if HELLS_GATE_PS5_NATIVE_GAME
#include "launcher/menu.h"
#include "renderer/renderer.h"
#include "renderer/renderer_config.h"
#endif

extern "C" [[noreturn]] void catchReturnFromMain(int status);
extern "C" void title_note(const char* format, ...) __attribute__((format(printf, 1, 2)));

namespace {

namespace fs = std::filesystem;

// DLC packages and pre-extracted content trees go in this folder, next to
// dump/ and tu/. Installed entries are listed in .installed so each package is
// only unpacked once.
constexpr const char* kDlcRoot = "/app0/dlc";
constexpr uint32_t kDantesInfernoTitleId = 0x454108CF;
constexpr uint32_t kMarketplaceContent = 0x00000002;

bool IsHexName(const std::string& name, size_t length) {
  return name.size() == length &&
         name.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
}

// Called while packages install: the action label, a status line and 0..1.
using DlcProgress =
    std::function<void(const std::string&, const std::string&, float)>;

struct PendingDlc {
  fs::path path;
  std::string rel;
  std::string name;
  uintmax_t size = 0;
};

// The package's English display name, read from the STFS header (UTF-16BE at
// 0x411). Falls back to the file name.
std::string DlcDisplayName(const std::array<uint8_t, 0x491>& header, const fs::path& path) {
  std::string name;
  for (size_t i = 0x411; i + 1 < 0x491; i += 2) {
    const uint32_t c = uint32_t(header[i]) << 8 | header[i + 1];
    if (c == 0) break;
    if (c < 0x80) {
      name += char(c);
    } else if (c < 0x800) {
      name += char(0xC0 | (c >> 6));
      name += char(0x80 | (c & 0x3F));
    } else {
      name += char(0xE0 | (c >> 12));
      name += char(0x80 | ((c >> 6) & 0x3F));
      name += char(0x80 | (c & 0x3F));
    }
  }
  return name.empty() ? path.filename().string() : name;
}

// Bytes written so far under `dir` (the package's install folder), to move the
// progress bar inside one large package.
uintmax_t FolderBytes(const fs::path& dir) {
  uintmax_t total = 0;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code size_ec;
    if (it->is_regular_file(size_ec)) {
      const auto size = it->file_size(size_ec);
      if (!size_ec) total += size;
    }
  }
  return total;
}

// Mirrors the desktop app's AutoInstallDlc (src/dantes_inferno_app.h). Uses the
// error_code overloads throughout: a filesystem exception here must not stop
// the game from booting without DLC. `progress` is called only when there is
// something to install, so normal launches never draw anything.
template <typename ContentManager>
void InstallDlc(ContentManager* content_manager, const fs::path& user_root,
                const DlcProgress& progress) {
  if (!content_manager) {
    title_note("dlc: skipped, content manager unavailable");
    return;
  }
  const fs::path root(kDlcRoot);
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    title_note("dlc: no %s folder, running without DLC", kDlcRoot);
    return;
  }

  const fs::path marker = root / ".installed";
  std::set<std::string> installed;
  {
    std::ifstream in(marker);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty()) installed.insert(line);
    }
  }

  // Content layout is user_root/<xuid>/<title_id>/<content_type>/. Folders named
  // like a XUID or a title ID (with Headers/, as Xenia stores them) are copied
  // as-is; every other file is treated as an STFS package.
  const fs::path content_root = user_root / "0000000000000000";
  std::vector<fs::path> packages;
  int mirrored = 0;
  for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
    const auto& entry = *it;
    std::error_code entry_ec;
    if (entry.is_regular_file(entry_ec)) {
      if (entry.path().filename() != ".installed") packages.push_back(entry.path());
      continue;
    }
    if (!entry.is_directory(entry_ec)) continue;

    const std::string name = entry.path().filename().string();
    fs::path dest;
    if (IsHexName(name, 16)) {
      dest = user_root / name;
    } else if (IsHexName(name, 8) && fs::exists(entry.path() / "Headers", entry_ec)) {
      dest = content_root / name;
    }
    if (!dest.empty()) {
      if (progress) progress("INSTALLING DLC", "Copying DLC content folder " + name + "...", 0);
      std::error_code copy_ec;
      fs::create_directories(dest, copy_ec);
      fs::copy(entry.path(), dest,
               fs::copy_options::recursive | fs::copy_options::update_existing, copy_ec);
      if (copy_ec) {
        title_note("dlc: copying %s failed: %s", name.c_str(), copy_ec.message().c_str());
      } else {
        title_note("dlc: copied content folder %s", name.c_str());
        ++mirrored;
      }
      continue;
    }

    std::error_code walk_ec;
    for (fs::recursive_directory_iterator sub(entry.path(), walk_ec), sub_end;
         !walk_ec && sub != sub_end; sub.increment(walk_ec)) {
      std::error_code file_ec;
      if (sub->is_regular_file(file_ec)) packages.push_back(sub->path());
    }
  }
  if (ec) title_note("dlc: could not read %s: %s", kDlcRoot, ec.message().c_str());

  // First pass: the packages still to install, so the progress bar knows the
  // total before anything is unpacked.
  std::vector<PendingDlc> pending;
  uintmax_t total_bytes = 0;
  for (const auto& package : packages) {
    std::error_code rel_ec;
    std::string rel = fs::relative(package, root, rel_ec).generic_string();
    if (rel_ec || rel.empty()) rel = package.generic_string();
    if (installed.count(rel)) continue;

    std::array<uint8_t, 0x491> header{};
    std::ifstream file(package, std::ios::binary);
    file.read(reinterpret_cast<char*>(header.data()), std::streamsize(header.size()));
    const bool complete = file.gcount() == std::streamsize(header.size());
    file.close();
    auto be32 = [&header](size_t offset) {
      return uint32_t(header[offset]) << 24 | uint32_t(header[offset + 1]) << 16 |
             uint32_t(header[offset + 2]) << 8 | uint32_t(header[offset + 3]);
    };
    const std::string magic(reinterpret_cast<const char*>(header.data()), 4);
    if (!complete || (magic != "LIVE" && magic != "PIRS" && magic != "CON ") ||
        be32(0x344) != kMarketplaceContent || be32(0x360) != kDantesInfernoTitleId) {
      title_note("dlc: skipped %s (not Dante's Inferno DLC: type %08X, title %08X)", rel.c_str(),
                 be32(0x344), be32(0x360));
      continue;
    }
    std::error_code size_ec;
    const auto size = fs::file_size(package, size_ec);
    PendingDlc item{package, rel, DlcDisplayName(header, package), size_ec ? 0 : size};
    total_bytes += item.size;
    pending.push_back(std::move(item));
  }

  // InstallContent unpacks into <user_root>/0000000000000000/454108CF/00000002/
  // <file name>/; watching that folder grows the bar inside a large package.
  const fs::path install_root = content_root / "454108CF" / "00000002";
  int added = 0;
  uintmax_t done_bytes = 0;
  for (size_t i = 0; i < pending.size(); ++i) {
    const auto& item = pending[i];
    const std::string status = "Installing " + item.name + " (" + std::to_string(i + 1) + " of " +
                               std::to_string(pending.size()) +
                               "). This happens once; please wait.";
    title_note("dlc: installing %s", item.rel.c_str());

    std::atomic<bool> finished{false};
    decltype(content_manager->InstallContent(item.path)) result{};
    std::thread worker([&] {
      result = content_manager->InstallContent(item.path);
      finished.store(true, std::memory_order_release);
    });
    const fs::path install_dir = install_root / item.path.filename();
    auto last_measure = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    uintmax_t written = 0;
    while (!finished.load(std::memory_order_acquire)) {
      const auto now = std::chrono::steady_clock::now();
      // Walking the folder costs a stat per file, so only every 2 seconds.
      if (item.size > (16u << 20) && now - last_measure > std::chrono::seconds(2)) {
        written = std::min(FolderBytes(install_dir), item.size);
        last_measure = now;
      }
      if (progress) {
        const double fraction =
            total_bytes ? double(done_bytes + written) / double(total_bytes) : 0.0;
        progress("INSTALLING DLC", status, float(fraction));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    worker.join();
    done_bytes += item.size;
    if (progress) {
      progress("INSTALLING DLC", status,
               total_bytes ? float(double(done_bytes) / double(total_bytes)) : 1.0f);
    }

    const std::string& rel = item.rel;
    if (XSUCCEEDED(result)) {
      installed.insert(rel);
      ++added;
      std::ofstream out(marker, std::ios::app);
      out << rel << '\n';
    } else {
      title_note("dlc: install of %s failed: 0x%08X", rel.c_str(), uint32_t(result));
    }
  }

  title_note("dlc: %d new packages, %d copied folders, %zu installed in total", added, mirrored,
             installed.size());
}

#if HELLS_GATE_PS5_NATIVE_GAME
class SyncOnlyGraphics final : public rex::graphics::vulkan::VulkanGraphicsSystem {
 public:
  rex::X_STATUS SetupPresentation(rex::ui::WindowedAppContext*) override {
    return 0;
  }
};
#endif

class HellsGateApp final : public rex::ReXApp {
 public:
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& context) {
    return std::unique_ptr<HellsGateApp>(new HellsGateApp(context));
  }

 private:
  explicit HellsGateApp(rex::ui::WindowedAppContext& context)
      : ReXApp(context, "dantes_inferno", PPCImageConfig) {}

  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)>) override {
    auto paths = defaults;
    const auto configured_game_data = rex::cvar::Query<std::string>("game_data_root");
    if (!configured_game_data.empty()) {
      paths.game_data_root = configured_game_data;
    }
    if (paths.game_data_root.empty()) {
      paths.game_data_root = "/app0/gamedata";
    }
#if HELLS_GATE_PS5_NATIVE_GAME
    try {
      render_mode_ = &hells_gate::ps5::LoadRenderMode();
      paths.game_data_root =
          hells_gate::ps5::RunLauncher(window(), paths.game_data_root, *render_mode_);
    } catch (const std::exception& error) {
      title_note("launcher: %s", error.what());
      catchReturnFromMain(1);
    }
    if (paths.game_data_root.empty()) catchReturnFromMain(0);
#endif
    for (const auto* name : {"default.xex", "default.xexp"}) {
      const auto path = paths.game_data_root / name;
      std::error_code error;
      if (!std::filesystem::is_regular_file(path, error)) {
        title_note("Missing required game file %s", path.c_str());
        catchReturnFromMain(1);
      }
    }
    return paths;
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    // Index-buffer tracing scans guest indices per draw and can flood the log.
    if (rex::cvar::GetFlagSource("vfetch_oob_trace") == rex::cvar::Source::kDefault) {
      rex::cvar::SetFlagByName("vfetch_oob_trace", "false");
    }
#if HELLS_GATE_PS5_NATIVE_GAME
    rex::cvar::SetFlagByName("renderer", "dante");
    renderer::ConfigureSyncOnlyGpu();
    config.graphics = std::make_unique<SyncOnlyGraphics>();
#else
    config.graphics = std::make_unique<rex::graphics::vulkan::VulkanGraphicsSystem>();
#endif
  }

  void OnPostSetup() override {
    // Runs before the guest module launches, so the game's first content
    // enumeration already sees the installed DLC. The native build shows the
    // install on the launcher screen, which it still owns at this point.
    DlcProgress progress;
#if HELLS_GATE_PS5_NATIVE_GAME
    progress = [](const std::string& title, const std::string& status, float fraction) {
      hells_gate::ps5::ShowLauncherStatus(title, status, fraction);
    };
#endif
    auto* kernel_state = runtime() ? runtime()->kernel_state() : nullptr;
    InstallDlc(kernel_state ? kernel_state->content_manager() : nullptr, user_data_root(),
               progress);
#if HELLS_GATE_PS5_NATIVE_GAME
    hells_gate::ps5::HandOffLauncher();
    StartRenderer();
#endif
  }

#if HELLS_GATE_PS5_NATIVE_GAME
  void StartRenderer() {
    renderer::SetButtonGlyphStyleSource([] { return renderer::GlyphStyle::kPlayStation5; });
    rex::cvar::SetFlagByName("dante_resolution", render_mode_->resolution);
    title_note("native game: starting Diligent renderer at %s", render_mode_->resolution);
    auto render_cache = cache_root();
    if (!render_cache.empty()) render_cache /= render_mode_->cache_directory;
    if (!renderer::Start(window(), render_cache)) {
      title_note("native game: FAIL renderer startup");
      catchReturnFromMain(1);
    }
    title_note("native game: renderer ready");
  }

  void OnShutdown() override {
    renderer::Stop();
  }

  const hells_gate::ps5::RenderMode* render_mode_ = &hells_gate::ps5::k1440p;
#endif

  void OnPreLaunchModule() override {
    auto* memory = runtime()->memory()->virtual_membase();
    if (rex::memory::load_and_swap<uint32_t>(memory + 0x82879110) != 0x3C8082CEu) {
      REXLOG_ERROR("Title Update 2 does not match the loaded default.xex");
      if (auto logger = rex::GetLogger()) logger->flush();
      catchReturnFromMain(1);
    }
    // The guest save-system setjmp must stay on the current fiber.
    *reinterpret_cast<uint32_t*>(memory + 0x82CE68E4u) = 0;
  }
};

}  // namespace

REX_DEFINE_APP(dantes_inferno, HellsGateApp::Create)
