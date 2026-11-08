#include <cstdint>
#include <memory>

#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/system/xmemory.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app.h>
#include <rex/ui/windowed_app_context.h>

extern "C" void title_note(const char* format, ...) __attribute__((format(printf, 1, 2)));

namespace {

class HellsGateSmoke final : public rex::ui::WindowedApp {
 public:
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& context) {
    return std::unique_ptr<HellsGateSmoke>(new HellsGateSmoke(context));
  }

 private:
  explicit HellsGateSmoke(rex::ui::WindowedAppContext& context)
      : WindowedApp(context, "hells_gate_smoke") {}

  bool OnInitialize() override {
    title_note("smoke: opening PS5 window");
    auto window = rex::ui::Window::Create(app_context(), "Hell's Gate PS5 Bring-up");
    if (!window || !window->Open()) {
      title_note("smoke: FAIL window");
      return false;
    }
    title_note("smoke: window %ux%u", window->GetActualPhysicalWidth(),
               window->GetActualPhysicalHeight());

    title_note("smoke: initializing Xbox 360 guest memory");
    auto memory = std::make_unique<rex::memory::Memory>();
    if (!memory->Initialize()) {
      title_note("smoke: FAIL guest memory");
      return false;
    }
    constexpr uint32_t kSentinel = 0x454108CF;
    const uint32_t address = memory->SystemHeapAlloc(sizeof(kSentinel));
    if (!address) {
      title_note("smoke: FAIL guest heap allocation");
      return false;
    }
    volatile uint32_t* value = memory->TranslateVirtual<uint32_t*>(address);
    *value = kSentinel;
    const bool read_back = *value == kSentinel;
    memory->SystemHeapFree(address);
    if (!read_back) {
      title_note("smoke: FAIL guest heap readback");
      return false;
    }
    title_note("smoke: guest memory readback passed");

    title_note("smoke: initializing static Xenos/RADV presenter");
    rex::graphics::vulkan::VulkanGraphicsSystem graphics;
    if (XFAILED(graphics.SetupPresentation(nullptr))) {
      title_note("smoke: FAIL Xenos/RADV presenter");
      return false;
    }
    title_note("smoke: Xenos/RADV presenter initialized");
    app_context().CallInUIThreadDeferred([this] { app_context().QuitFromUIThread(); });
    title_note("smoke: PASS");
    return true;
  }
};

}  // namespace

REX_DEFINE_APP(hells_gate_smoke, HellsGateSmoke::Create)
