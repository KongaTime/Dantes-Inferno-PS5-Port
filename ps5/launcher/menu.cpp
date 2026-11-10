#include "menu.h"

#include "platform.h"
#include "prepare_game.h"
#include "renderer/renderer.h"
#include "renderer/ui_overlay.h"
#include "renderer/vulkan_device.h"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

#include <imgui.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/presenter.h>
#include <rex/ui/window.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

extern "C" void title_note(const char* format, ...) __attribute__((format(printf, 1, 2)));

namespace hells_gate::ps5 {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr uint64_t kBaseXexHash = 0x223e3cef15ccbc9dULL;
constexpr uint64_t kTitleUpdateHash = 0x54cb9c012199af3bULL;
constexpr uint64_t kTitleUpdateSize = 2205696;

class Menu final : public rex::ui::ImGuiDialog {
 public:
  Menu(rex::ui::ImGuiDrawer* drawer, renderer::OverlayDrawer& overlay) : ImGuiDialog(drawer) {
    int width = 0, height = 0, channels = 0;
    auto* pixels = stbi_load("/app0/launcher.png", &width, &height, &channels, 4);
    if (pixels) {
      background_ = overlay.CreateTexture(width, height, rex::ui::ImmediateTextureFilter::kLinear,
                                          false, pixels);
      stbi_image_free(pixels);
    }
  }

  bool ready = false;
  bool exit_selected = false;
  bool preparing = false;
  float progress = 0;
  std::string status;

 private:
  void OnDraw(ImGuiIO& io) override {
    const auto size = io.DisplaySize;
    auto* draw = ImGui::GetForegroundDrawList();
    auto at = [&](float x, float y) { return ImVec2(size.x * x, size.y * y); };
    auto text = [&](float x, float y, float height, ImU32 color, const char* label) {
      draw->AddText(ImGui::GetFont(), size.y * height, at(x, y), color, label);
    };
    const ImU32 white = IM_COL32(245, 232, 218, 255);
    const ImU32 gold = IM_COL32(255, 213, 134, 255);
    if (background_) {
      draw->AddImage(reinterpret_cast<ImTextureID>(background_.get()), {0, 0}, size);
    } else {
      text(.07f, .13f, .067f, white, "DANTE'S INFERNO");
      text(.073f, .22f, .035f, gold, "RECOMP");
    }

    // Cover the mockup's baked-in labels with the current interactive state.
    draw->AddRectFilled(at(0, .418f), at(.39f, .637f), IM_COL32(30, 17, 14, 255));
    const float selected_y = exit_selected ? .496f : .428f;
    draw->AddRectFilled(at(.035f, selected_y), at(.35f, selected_y + .056f),
                        IM_COL32(100, 60, 29, 255), size.y * .005f);
    const char* action = preparing ? "PREPARING GAME" : ready ? "PLAY GAME" : "PREPARE GAME";
    text(.082f, .435f, .038f, exit_selected ? white : gold, action);
    text(.082f, .503f, .038f, exit_selected ? gold : white, "EXIT");
    if (!status.empty()) {
      draw->AddText(ImGui::GetFont(), size.y * .021f, at(.046f, .567f), white,
                    status.c_str(), nullptr, size.x * .315f);
    }
    if (preparing) {
      draw->AddRectFilled(at(.046f, .619f), at(.35f, .626f), IM_COL32(80, 59, 46, 255));
      draw->AddRectFilled(at(.046f, .619f), at(.046f + .304f * progress, .626f), gold);
    }
  }

  std::unique_ptr<rex::ui::ImmediateTexture> background_;
};

PrepareConfig LocalPrepareConfig() {
  PrepareConfig config;
  config.iso_roots = {"/app0/dump", "/app0"};
  config.update = "/app0/tu/default.xexp";
  config.update_fallback = "/app0/default.xexp";
  config.base_xex_hash = kBaseXexHash;
  config.update_hash = kTitleUpdateHash;
  config.update_size = kTitleUpdateSize;
  return config;
}

}  // namespace

fs::path RunLauncher(rex::ui::Window* window, const fs::path& configured_game) {
  constexpr const char* kFallbackGame = "/app0/gamedata";
  // Prepare installs into the root this title launches from (game_data_root),
  // not a second copy inside the app folder; the app-local folder is only the
  // fallback when no root was configured.
  const fs::path destination =
      configured_game.empty() ? fs::path(kFallbackGame) : configured_game;
  auto prepared = gameReady(configured_game) ? configured_game : fs::path(kFallbackGame);
  // Heap-allocated so the game renderer can take over the very same swap chain.
  auto device = std::make_unique<renderer::VulkanDevice>();
  if (!device->Initialize(nullptr, 1280, 720)) {
    throw std::runtime_error("Could not initialize the PS5 launcher display.");
  }
  renderer::OverlayRenderer overlay_renderer(device.get());
  renderer::OverlayDrawer overlay_drawer;
  rex::ui::ImGuiDrawer drawer(window, 64);
  drawer.SetImmediateDrawer(&overlay_drawer);
  Menu menu(&drawer, overlay_drawer);
  menu.ready = gameReady(prepared);
  menu.status = menu.ready ? "Cross: Play     Circle: Exit" : "Cross: Prepare     Circle: Exit";
  device->SetBeforePresent([&](auto* back_buffer) {
    overlay_renderer.Draw(device->Context(), back_buffer);
  });
  auto paint = [&] {
    rex::ui::AppUIDrawContext context(window->GetActualPhysicalWidth(),
                                      window->GetActualPhysicalHeight());
    drawer.Draw(context);
    device->BeginFrame();
    device->Clear(.06f, .03f, .02f, 1.0f);
    device->Present(1);
  };

  struct pad input{};
  pad_poll(&input);
  title_note("launcher: %s", menu.ready ? "Play Game" : "Prepare Game");
  while (true) {
    paint();
    pad_poll(&input);
    if (input.pressed & (PAD_UP | PAD_DOWN)) menu.exit_selected = !menu.exit_selected;
    if ((input.pressed & PAD_CIRCLE) || (menu.exit_selected && (input.pressed & PAD_CROSS))) {
      return {};
    }
    if (!(input.pressed & PAD_CROSS)) continue;
    if (menu.ready) {
      // The game renderer reads the same overlay frame mailbox.
      overlay_drawer.PublishEmpty();
      // Present the game on this swap chain. A swap chain created after this
      // one reports successful flips but never reaches the panel, which leaves
      // this menu frame frozen on screen while the game runs behind it.
      device->SetBeforePresent(nullptr);
      renderer::AdoptDevice(std::move(device));
      title_note("launcher: handed the swap chain to the game renderer");
      return prepared;
    }
    menu.preparing = true;
    menu.status = "Checking disc and title update...";
    menu.progress = 0;
    paint();
    auto last_paint = Clock::now();
    try {
      prepareGame(LocalPrepareConfig(), destination, [&](uint64_t done, uint64_t total,
                                                        const std::string&) {
        auto now = Clock::now();
        if (now - last_paint < std::chrono::milliseconds(100)) return;
        last_paint = now;
        menu.progress = total ? float(double(done) / double(total)) : 0;
        menu.status = "Extracting disc... " + std::to_string(int(menu.progress * 100)) + "%";
        paint();
        pad_poll(&input);
      });
      prepared = destination;
      menu.ready = true;
      menu.status = "Game ready. Press Cross to play.";
      title_note("launcher: preparation complete");
    } catch (const std::exception& error) {
      menu.status = error.what();
      title_note("launcher: preparation failed: %s", error.what());
    }
    menu.preparing = false;
    pad_poll(&input);
  }
}

}  // namespace hells_gate::ps5
