set(TITLE_RESULTS_FILE /app0/hells-gate-native-results.txt)
set(diligent_root "$ENV{HELLS_GATE_DILIGENT_ROOT}")
set(diligent_build "${diligent_root}/diligent-ps5")
if(NOT EXISTS "${diligent_build}/Graphics/GraphicsEngineVulkan/libDiligent-GraphicsEngineVk-static.a")
    message(FATAL_ERROR "Build DiligentCore first with bash ps5/build.sh native-deps")
endif()

add_library(hells_gate_native_smoke OBJECT "${CMAKE_CURRENT_LIST_DIR}/smoke.cpp")
rex_ps5_settings(hells_gate_native_smoke)
target_compile_definitions(hells_gate_native_smoke PRIVATE PLATFORM_LINUX=1)
target_include_directories(hells_gate_native_smoke PRIVATE "${diligent_root}/diligent-core")
rex_ps5_link_objects(hells_gate_native_smoke)

# The title template supplies Volk. Diligent's Vulkan calls use those same
# dispatch symbols, loaded from the RADV driver linked into the title.
set(diligent_archives
    Graphics/GraphicsEngineVulkan/libDiligent-GraphicsEngineVk-static.a
    Graphics/GraphicsTools/libDiligent-GraphicsTools.a
    Graphics/GraphicsEngine/libDiligent-GraphicsEngine.a
    Graphics/GraphicsAccessories/libDiligent-GraphicsAccessories.a
    Graphics/GraphicsEngineNextGenBase/libDiligent-GraphicsEngineNextGenBase.a
    Graphics/ShaderTools/libDiligent-ShaderTools.a
    Common/libDiligent-Common.a
    Platforms/Linux/libDiligent-LinuxPlatform.a
    Platforms/Basic/libDiligent-BasicPlatform.a
    Primitives/libDiligent-Primitives.a
    ThirdParty/SPIRV-Cross/libspirv-cross-core.a)
list(TRANSFORM diligent_archives PREPEND "${diligent_build}/")
set_property(GLOBAL APPEND PROPERTY TITLE_LINK_INPUTS --start-group ${diligent_archives} --end-group)
set_property(GLOBAL APPEND PROPERTY TITLE_LINK_DEPENDS
    ${diligent_archives} "${CMAKE_CURRENT_LIST_DIR}/notices.json")
