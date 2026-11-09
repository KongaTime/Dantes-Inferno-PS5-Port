#include <cstdint>
#include <memory>

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "Graphics/GraphicsTools/interface/OffScreenSwapChain.hpp"

#include <rex/ui/windowed_app.h>
#include <rex/ui/windowed_app_context.h>

extern "C" void title_note(const char* format, ...) __attribute__((format(printf, 1, 2)));

namespace {

class NativeSmoke final : public rex::ui::WindowedApp {
 public:
  explicit NativeSmoke(rex::ui::WindowedAppContext& context)
      : WindowedApp(context, "hells_gate_native_smoke") {}

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& context) {
    return std::make_unique<NativeSmoke>(context);
  }

 private:
  bool OnInitialize() override {
    using namespace Diligent;
    title_note("native smoke: creating Diligent Vulkan device through RADV");
    auto* factory = GetEngineFactoryVk();
    EngineVkCreateInfo engine;
    engine.GraphicsAPIVersion = Version{1, 3};
    engine.EnableValidation = false;
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> context;
    factory->CreateDeviceAndContextsVk(engine, &device, &context);
    if (!device || !context) {
      title_note("native smoke: FAIL Vulkan device or context");
      return false;
    }

    SwapChainDesc desc;
    desc.Width = 64;
    desc.Height = 64;
    desc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
    desc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
    RefCntAutoPtr<ISwapChain> target;
    CreateOffScreenSwapChain(device, context, desc, &target);
    auto* view = target ? target->GetCurrentBackBufferRTV() : nullptr;
    if (!view) {
      title_note("native smoke: FAIL offscreen render target");
      return false;
    }

    auto* source = view->GetTexture();
    TextureDesc staging_desc = source->GetDesc();
    staging_desc.Name = "Native smoke readback";
    staging_desc.Usage = USAGE_STAGING;
    staging_desc.BindFlags = BIND_NONE;
    staging_desc.CPUAccessFlags = CPU_ACCESS_READ;
    RefCntAutoPtr<ITexture> staging;
    device->CreateTexture(staging_desc, nullptr, &staging);
    if (!staging) {
      title_note("native smoke: FAIL staging texture");
      return false;
    }

    constexpr auto transition = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    constexpr float color[] = {0.0f, 1.0f, 0.0f, 1.0f};
    context->SetRenderTargets(1, &view, nullptr, transition);
    context->ClearRenderTarget(view, color, transition);
    context->SetRenderTargets(0, nullptr, nullptr, transition);
    context->CopyTexture(CopyTextureAttribs{source, transition, staging, transition});
    context->Flush();
    context->WaitForIdle();
    MappedTextureSubresource mapped{};
    context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
    const auto* pixel = static_cast<const uint8_t*>(mapped.pData);
    if (!pixel) {
      title_note("native smoke: FAIL map readback");
      return false;
    }
    const bool passed = pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255;
    title_note("native smoke: RGBA readback %u %u %u %u", unsigned(pixel[0]), unsigned(pixel[1]),
               unsigned(pixel[2]), unsigned(pixel[3]));
    context->UnmapTextureSubresource(staging, 0, 0);
    if (!passed) {
      title_note("native smoke: FAIL unexpected clear color");
      return false;
    }
    title_note("native smoke: PASS");
    app_context().CallInUIThreadDeferred([this] { app_context().QuitFromUIThread(); });
    return true;
  }
};

}  // namespace

REX_DEFINE_APP(hells_gate_native_smoke, NativeSmoke::Create)
