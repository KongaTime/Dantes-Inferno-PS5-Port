set(TITLE_RESULTS_FILE /app0/hells-gate-native-results.txt)
set(diligent_root "$ENV{HELLS_GATE_DILIGENT_ROOT}")
set(diligent_build "${diligent_root}/diligent-ps5")
if(NOT EXISTS "${diligent_build}/Graphics/GraphicsEngineVulkan/libDiligent-GraphicsEngineVk-static.a")
    message(FATAL_ERROR "Build DiligentCore first with bash ps5/build.sh native-deps")
endif()

option(HELLS_GATE_NATIVE_GAME "Build Dante gameplay with the native renderer" OFF)
if(HELLS_GATE_NATIVE_GAME)
    include("${CMAKE_CURRENT_LIST_DIR}/../game.cmake")
    set(TITLE_RESULTS_FILE /app0/hells-gate-native-results.txt)
    set(renderer_dir "${HELLS_GATE_ROOT}/src/renderer")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
    set(ps5_spirv "${CMAKE_CURRENT_BINARY_DIR}/ps5_native_spirv.h")
    add_custom_command(OUTPUT "${ps5_spirv}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/compile_shaders.py"
            "${HELLS_GATE_ROOT}" "${GLSLANG_VALIDATOR}" "${ps5_spirv}"
        DEPENDS "${CMAKE_CURRENT_LIST_DIR}/compile_shaders.py"
            "${renderer_dir}/vulkan_device.cpp" "${renderer_dir}/renderer.cpp"
            "${renderer_dir}/ui_overlay.cpp" "${renderer_dir}/smaa_pass.cpp"
            "${HELLS_GATE_ROOT}/thirdparty/smaa/SMAA.hlsl"
        VERBATIM)
    target_sources(hells_gate_app PRIVATE
        "${renderer_dir}/renderer.cpp"
        "${renderer_dir}/draw_renderer.cpp"
        "${renderer_dir}/pipeline_storage.cpp"
        "${renderer_dir}/png_dump.cpp"
        "${renderer_dir}/button_glyphs.cpp"
        "${renderer_dir}/shader_cache.cpp"
        "${renderer_dir}/smaa_pass.cpp"
        "${renderer_dir}/texture_cache.cpp"
        "${renderer_dir}/mod_loader.cpp"
        "${renderer_dir}/thread_cpu.cpp"
        "${renderer_dir}/renderer_config.cpp"
        "${renderer_dir}/ui_overlay.cpp"
        "${renderer_dir}/vulkan_device.cpp"
        "${renderer_dir}/d3d_hooks.cpp"
        "${renderer_dir}/renderdoc_capture.cpp"
        "${renderer_dir}/state_verifier.cpp"
        "${renderer_dir}/guest_gpu_state.cpp"
        "${ps5_spirv}")
    set(native_target hells_gate_app)
    target_compile_definitions(hells_gate_app PRIVATE HELLS_GATE_PS5_NATIVE_GAME=1)
    target_include_directories(hells_gate_app PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${REXGLUE_SRC}/thirdparty/glslang"
        "${diligent_root}/diligent-core/ThirdParty/stb")
else()
    add_library(hells_gate_native_smoke OBJECT "${CMAKE_CURRENT_LIST_DIR}/smoke.cpp")
    rex_ps5_settings(hells_gate_native_smoke)
    rex_ps5_link_objects(hells_gate_native_smoke)
    set(native_target hells_gate_native_smoke)
endif()
target_compile_definitions(${native_target} PRIVATE PLATFORM_LINUX=1)
target_include_directories(${native_target} PRIVATE "${diligent_root}/diligent-core")

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
