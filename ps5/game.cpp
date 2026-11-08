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

extern "C" [[noreturn]] void catchReturnFromMain(int status);
extern "C" void title_note(const char* format, ...) __attribute__((format(printf, 1, 2)));

namespace {

class HellsGateApp final : public rex::ReXApp {
 public:
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& context) {
    return std::unique_ptr<HellsGateApp>(new HellsGateApp(context));
  }

 private:
  explicit HellsGateApp(rex::ui::WindowedAppContext& context)
      : ReXApp(context, "dantes_inferno", PPCImageConfig) {}

  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (paths.game_data_root.empty()) {
      paths.game_data_root = "/app0/gamedata";
    }
    for (const auto* name : {"default.xex", "default.xexp"}) {
      const auto path = paths.game_data_root / name;
      std::error_code error;
      if (!std::filesystem::is_regular_file(path, error)) {
        title_note("Missing required game file %s", path.c_str());
        catchReturnFromMain(1);
      }
    }
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    // Index-buffer tracing scans guest indices per draw and can flood the log.
    if (rex::cvar::GetFlagSource("vfetch_oob_trace") == rex::cvar::Source::kDefault) {
      rex::cvar::SetFlagByName("vfetch_oob_trace", "false");
    }
    config.graphics = std::make_unique<rex::graphics::vulkan::VulkanGraphicsSystem>();
  }

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
