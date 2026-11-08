get_filename_component(HELLS_GATE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(TITLE_RESULTS_FILE /app0/hells-gate-results.txt)

option(HELLS_GATE_SMOKE "Build the game-data-free PS5 smoke test" ON)
if(HELLS_GATE_SMOKE)
    add_library(hells_gate_smoke OBJECT "${CMAKE_CURRENT_LIST_DIR}/smoke.cpp")
    rex_ps5_settings(hells_gate_smoke)
    target_include_directories(hells_gate_smoke PRIVATE
        "${REXGLUE_SRC}/thirdparty/vulkan-headers/include"
        "${REXGLUE_SRC}/thirdparty/renderdoc")
    rex_ps5_link_objects(hells_gate_smoke)
else()
    if(NOT EXISTS "${rex_runtime_dir}/librexgpu-xenos.a")
        message(FATAL_ERROR "Build rexgpu-xenos alongside rexruntime for the PS5")
    endif()
    rex_ps5_module(hells_gate_guest
        GENERATED "${HELLS_GATE_ROOT}/generated/default"
        INCLUDES "${HELLS_GATE_ROOT}" "${HELLS_GATE_ROOT}/src")
    add_library(hells_gate_app OBJECT
        "${CMAKE_CURRENT_LIST_DIR}/game.cpp"
        "${REX_APP_SOURCE}"
        "${HELLS_GATE_ROOT}/src/frame_rate_hooks.cpp"
        "${HELLS_GATE_ROOT}/src/memory_hooks.cpp"
        "${HELLS_GATE_ROOT}/src/menu_hooks.cpp")
    target_include_directories(hells_gate_app PRIVATE
        "${HELLS_GATE_ROOT}" "${HELLS_GATE_ROOT}/src"
        "${HELLS_GATE_ROOT}/generated/default"
        "${REXGLUE_SRC}/thirdparty/vulkan-headers/include"
        "${REXGLUE_SRC}/thirdparty/renderdoc")
    rex_ps5_settings(hells_gate_app)
    rex_ps5_link_objects(hells_gate_app)
endif()
