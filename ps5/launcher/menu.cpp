#include "menu.h"

#include "platform.h"
#include "prepare_game.h"
#include "renderer/renderer.h"
#include "renderer/ui_overlay.h"
#include "renderer/vulkan_device.h"

#include <array>
#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#include <imgui.h>
#include <rex/filesystem.h>
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

enum class MenuRow { kGame, kResolution, kExit };

fs::path ResolutionPreference() {
  return rex::filesystem::GetUserFolder() / "dante-render-resolution.txt";
}

bool SaveRenderMode(const RenderMode& mode) {
  const auto path = ResolutionPreference();
  std::error_code error;
  fs::create_directories(path.parent_path(), error);
  if (error) return false;
  auto temporary = path;
  temporary += ".tmp";
  std::ofstream file(temporary, std::ios::trunc);
  file << mode.resolution << '\n';
  file.close();
  if (!file) return false;
  fs::rename(temporary, path, error);
  if (!error) return true;
  fs::remove(temporary, error);
  return false;
}

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
  MenuRow row = MenuRow::kGame;
  const RenderMode* preferred_mode = &k1440p;
  bool preparing = false;
  float progress = 0;
  std::string status;
  // Replaces the first row's label while work runs after Play (DLC install).
  std::string action_override;

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
    draw->AddRectFilled(at(0, .418f), at(.39f, .692f), IM_COL32(30, 17, 14, 255));
    const char* action = !action_override.empty() ? action_override.c_str()
                         : preparing              ? "PREPARING GAME"
                         : ready                  ? "PLAY GAME"
                                                  : "PREPARE GAME";
    const std::string resolution = std::string("RESOLUTION   ") + preferred_mode->label;
    const std::array labels{action, resolution.c_str(), "EXIT"};
    for (size_t i = 0; i < labels.size(); ++i) {
      const float y = .428f + float(i) * .059f;
      const bool selected = i == static_cast<size_t>(row);
      if (selected) {
        draw->AddRectFilled(at(.035f, y), at(.35f, y + .053f),
                            IM_COL32(100, 60, 29, 255), size.y * .005f);
      }
      text(.065f, y + .007f, .033f, selected ? gold : white, labels[i]);
    }
    if (!status.empty()) {
      draw->AddText(ImGui::GetFont(), size.y * .021f, at(.046f, .609f), white,
                    status.c_str(), nullptr, size.x * .315f);
    }
    if (preparing) {
      draw->AddRectFilled(at(.046f, .672f), at(.35f, .679f), IM_COL32(80, 59, 46, 255));
      draw->AddRectFilled(at(.046f, .672f), at(.046f + .304f * progress, .679f), gold);
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

const RenderMode& LoadRenderMode() {
  std::ifstream file(ResolutionPreference());
  std::string value;
  return std::getline(file, value) && value == k4K.resolution ? k4K : k1440p;
}

// Everything the launcher draws with. It lives past RunLauncher so the menu can
// show work done after Play (the DLC install) before the game takes the swap
// chain. Members are declared in the order the launcher used to create them, so
// they are destroyed in reverse, as before.
namespace {

struct LauncherSession {
  LauncherSession(rex::ui::Window* window, const RenderMode& mode) : window(window) {
    // Heap-allocated so the game renderer can take over the very same swap chain.
    device = std::make_unique<renderer::VulkanDevice>();
    if (!device->Initialize(nullptr, mode.width, mode.height)) {
      throw std::runtime_error("Could not initialize the PS5 launcher display.");
    }
    overlay_renderer = std::make_unique<renderer::OverlayRenderer>(device.get());
    drawer = std::make_unique<rex::ui::ImGuiDrawer>(window, 64);
    drawer->SetImmediateDrawer(&overlay_drawer);
    menu = std::make_unique<Menu>(drawer.get(), overlay_drawer);
    device->SetBeforePresent([this](auto* back_buffer) {
      overlay_renderer->Draw(device->Context(), back_buffer);
    });
  }

  void Paint() {
    rex::ui::AppUIDrawContext context(window->GetActualPhysicalWidth(),
                                      window->GetActualPhysicalHeight());
    drawer->Draw(context);
    device->BeginFrame();
    device->Clear(.06f, .03f, .02f, 1.0f);
    device->Present(1);
  }

  void HandOff() {
    // The game renderer reads the same overlay frame mailbox.
    overlay_drawer.PublishEmpty();
    // Present the game on this swap chain. A swap chain created after this
    // one reports successful flips but never reaches the panel, which leaves
    // this menu frame frozen on screen while the game runs behind it.
    device->SetBeforePresent(nullptr);
    renderer::AdoptDevice(std::move(device));
    title_note("launcher: handed the swap chain to the game renderer");
  }

  rex::ui::Window* window;
  std::unique_ptr<renderer::VulkanDevice> device;
  std::unique_ptr<renderer::OverlayRenderer> overlay_renderer;
  renderer::OverlayDrawer overlay_drawer;
  std::unique_ptr<rex::ui::ImGuiDrawer> drawer;
  std::unique_ptr<Menu> menu;
};

// Set when the player chooses Play; cleared by HandOffLauncher.
std::unique_ptr<LauncherSession> g_session;

}  // namespace

void ShowLauncherStatus(const std::string& title, const std::string& status, float progress) {
  if (!g_session) return;
  auto& menu = *g_session->menu;
  menu.row = MenuRow::kGame;
  menu.action_override = title;
  menu.status = status;
  menu.preparing = true;
  menu.progress = progress < 0 ? 0 : progress > 1 ? 1 : progress;
  g_session->Paint();
}

void HandOffLauncher() {
  if (!g_session) return;
  g_session->HandOff();
  g_session.reset();
}

fs::path RunLauncher(rex::ui::Window* window, const fs::path& configured_game,
                     const RenderMode& mode) {
  constexpr const char* kFallbackGame = "/app0/gamedata";
  // Prepare installs into the root this title launches from (game_data_root),
  // not a second copy inside the app folder; the app-local folder is only the
  // fallback when no root was configured.
  const fs::path destination =
      configured_game.empty() ? fs::path(kFallbackGame) : configured_game;
  auto prepared = gameReady(configured_game) ? configured_game : fs::path(kFallbackGame);
  auto session = std::make_unique<LauncherSession>(window, mode);
  Menu& menu = *session->menu;
  menu.preferred_mode = &mode;
  menu.ready = gameReady(prepared);
  menu.status = menu.ready ? "Cross: Play     Circle: Exit" : "Cross: Prepare     Circle: Exit";
  auto paint = [&] { session->Paint(); };

  struct pad input{};
  pad_poll(&input);
  title_note("launcher: active render mode %s", mode.resolution);
  title_note("launcher: %s", menu.ready ? "Play Game" : "Prepare Game");
  while (true) {
    paint();
    pad_poll(&input);
    if (input.pressed & PAD_UP) {
      menu.row = static_cast<MenuRow>((static_cast<int>(menu.row) + 2) % 3);
    } else if (input.pressed & PAD_DOWN) {
      menu.row = static_cast<MenuRow>((static_cast<int>(menu.row) + 1) % 3);
    }
    const bool confirm = input.pressed & PAD_CROSS;
    if ((input.pressed & PAD_CIRCLE) || (menu.row == MenuRow::kExit && confirm)) return {};
    if (menu.row == MenuRow::kResolution &&
        (confirm || (input.pressed & (PAD_LEFT | PAD_RIGHT)))) {
      const auto& next = menu.preferred_mode == &k4K ? k1440p : k4K;
      if (SaveRenderMode(next)) {
        menu.preferred_mode = &next;
        title_note("launcher: saved render mode %s", next.resolution);
        menu.status = &next == &mode ? "Using this resolution."
                                    : std::string("Saved. Reopen the app to apply ") + next.label + ".";
      } else {
        menu.status = "Could not save the resolution.";
      }
      continue;
    }
    if (!confirm || menu.row != MenuRow::kGame) continue;
    if (menu.preferred_mode != &mode) {
      menu.status = "Reopen the app to apply the saved resolution.";
      continue;
    }
    if (menu.ready) {
      // Keep the screen until the game hands off (HandOffLauncher), so work
      // before the first game frame can show progress here.
      menu.status = "Starting...";
      paint();
      g_session = std::move(session);
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
