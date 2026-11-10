#include "generated/default/dantes_inferno_init.h"

#include <filesystem>
#include <memory>

#include <rex/cvar.h>
#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/logging/macros.h>
#include <rex/memory/utils.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

#if HELLS_GATE_PS5_NATIVE_GAME
#include "launcher/menu.h"
#include "renderer/renderer.h"
#include "renderer/renderer_config.h"
#endif

extern "C" [[noreturn]] void catchReturnFromMain(int status);
extern "C" void title_note(const char* format, ...) __attribute__((format(printf, 1, 2)));

namespace {

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

#if HELLS_GATE_PS5_NATIVE_GAME
  void OnPostSetup() override {
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
