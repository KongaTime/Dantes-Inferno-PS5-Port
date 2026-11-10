#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mode=${1:-smoke}
if [[ $# -gt 1 || ! $mode =~ ^(runtime|host|codegen|native-deps|native-smoke|native-game|smoke|game)$ ]]; then
    echo "Usage: bash ps5/build.sh [runtime|host|codegen|native-deps|native-smoke|native-game|smoke|game]" >&2
    exit 2
fi

fail() { printf '%s\n' "$*" >&2; exit 1; }

build=${HELLS_GATE_BUILD_DIR:-"$root/out/ps5"}
mkdir -p "$build"
build=$(cd -- "$build" && pwd)

if [[ $mode == codegen ]]; then
    for file in default.xex default.xexp; do
        [[ -s $root/game/$file ]] || fail "Missing game/$file. Extract Dante's Inferno and supply the matching Title Update 2."
    done
    codegen=${REXGLUE_CODEGEN:-"${REXGLUE_SRC:-$build/rex}/out/linux-amd64/rexglue"}
    [[ -x $codegen ]] || fail "Host ReXGlue CLI missing. Build it with: bash ps5/build.sh host"
    codegen=$(realpath "$codegen")
    cd "$root"
    "$codegen" codegen --ignore-stamp dantes_inferno_manifest.toml
    python3 patches/generated/apply_generated_patches.py
    exit
fi

if [[ $mode =~ ^(game|native-game)$ && ! -s $root/generated/default/sources.cmake ]]; then
    fail "Missing recompiled guest sources. Run: bash ps5/build.sh codegen"
fi

export REXGLUE_SRC=${REXGLUE_SRC:-"$build/rex"}
export REXGLUE_PS5_BUILD=${REXGLUE_PS5_BUILD:-"$build/runtime"}
export PS5_CLANG=${PS5_CLANG:-clang-21}
sdk_pin=f47ccdde34b3a992f7872a27619fc588040c229f
if [[ $mode == runtime || $mode == host ]]; then
    if [[ ! -d $REXGLUE_SRC ]]; then
        git clone --no-checkout "${REXGLUE_FORK:-https://github.com/KongaTime/rexglue-sdk}" "$REXGLUE_SRC"
        git -C "$REXGLUE_SRC" switch --detach "$sdk_pin"
    fi
    [[ $(git -C "$REXGLUE_SRC" rev-parse HEAD) == "$sdk_pin" ]] ||
        fail "REXGLUE_SRC must be an isolated checkout at $sdk_pin"
    patch="$root/ps5/rexglue-ps5.patch"
    if ! git -C "$REXGLUE_SRC" apply --reverse --check "$patch" 2>/dev/null; then
        git -C "$REXGLUE_SRC" apply --check "$patch"
        git -C "$REXGLUE_SRC" apply "$patch"
    fi
    submodules=(cli11 libmspack FFmpeg tomlplusplus simde xxHash spdlog fmt utfcpp imgui sdl3 o1heap
        vulkan-headers vulkan-memory-allocator spirv-headers spirv-tools glslang)
    [[ $mode != host ]] || submodules+=(inja)
    for submodule in "${submodules[@]}"; do
        [[ -e $REXGLUE_SRC/thirdparty/$submodule/.git ]] ||
            git -C "$REXGLUE_SRC" submodule update --init --depth 1 "thirdparty/$submodule"
    done
fi

common=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF)
if [[ $mode == host ]]; then
    cmake -S "$REXGLUE_SRC" -B "$build/host" "${common[@]}" \
        -DCMAKE_C_COMPILER="${CC:-clang-21}" -DCMAKE_CXX_COMPILER="${CXX:-clang++-21}" \
        "-DCMAKE_CXX_FLAGS=-mavx2 -I$REXGLUE_SRC/thirdparty/imgui" \
        -DSDL_X11_XTEST=OFF -DSDL_WAYLAND_LIBDECOR=OFF
    cmake --build "$build/host" --target rexglue
    exit
fi

: "${PS5_PAYLOAD_SDK:?Set PS5_PAYLOAD_SDK to the installed payload SDK with the PS5 platform layer}"
toolchain="$PS5_PAYLOAD_SDK/toolchain/prospero.cmake"
[[ -f $toolchain ]] || fail "Missing $toolchain"
if [[ $mode == native-deps ]]; then
    diligent="$build/diligent-core"
    diligent_pin=2c81c11ab3e293f3a54c13c6fec5b4b3dae5b974
    if [[ ! -e $diligent ]]; then
        git init "$diligent"
        git -C "$diligent" remote add origin https://github.com/DiligentGraphics/DiligentCore.git
    fi
    if ! git -C "$diligent" rev-parse --verify HEAD >/dev/null 2>&1; then
        git -C "$diligent" fetch --depth 1 origin "$diligent_pin"
        git -C "$diligent" switch --detach FETCH_HEAD
    fi
    [[ $(git -C "$diligent" rev-parse HEAD) == "$diligent_pin" ]] ||
        fail "DiligentCore must be an isolated checkout at $diligent_pin"
    for patch in "$root/patches/diligent/diligent-core.patch" "$root/ps5/diligent-core-ps5.patch"; do
        if ! git -C "$diligent" apply --reverse --check "$patch" 2>/dev/null; then
            git -C "$diligent" apply "$patch"
        fi
    done
    git -C "$diligent" submodule update --init --depth 1 --jobs 5 \
        ThirdParty/{SPIRV-Cross,SPIRV-Headers,Vulkan-Headers,volk,xxHash}
    cmake -S "$diligent" -B "$build/diligent-ps5" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DDILIGENT_NO_OPENGL=ON -DDILIGENT_NO_WEBGPU=ON -DDILIGENT_NO_ARCHIVER=ON \
        -DDILIGENT_NO_HLSL=ON -DDILIGENT_NO_GLSLANG=ON -DDILIGENT_INSTALL_CORE=OFF \
        -DDILIGENT_BUILD_TESTS=OFF
    cmake --build "$build/diligent-ps5" --target Diligent-GraphicsEngineVk-static Diligent-GraphicsTools
    exit
fi

if [[ $mode == runtime ]]; then
    cmake -S "$REXGLUE_SRC" -B "$REXGLUE_PS5_BUILD" "${common[@]}" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        '-DCMAKE_C_FLAGS=-march=znver2 -fno-omit-frame-pointer' \
        '-DCMAKE_CXX_FLAGS=-march=znver2 -fno-omit-frame-pointer'
    cmake --build "$REXGLUE_PS5_BUILD" --target rexruntime rexgpu-xenos
    exit
fi

: "${PS5_REXGLUE_TEMPLATE:?Set PS5_REXGLUE_TEMPLATE to a PS5_ReXGlueTemplate checkout}"
: "${PS5_VULKAN_DIR:?Set PS5_VULKAN_DIR to a prepared PS5_Vulkan checkout}"
# The template writes dist/ beside its sources. Give this project a separate
# checkout so its build cannot overwrite another project's local package.
template="$build/title-template"
title_build="$build/title"
title_game="$root/ps5"
native_game=OFF
if [[ $mode == native-smoke || $mode == native-game ]]; then
    template="$build/native-title-template"
    title_build="$build/native-title"
    title_game="$root/ps5/native-smoke"
    export HELLS_GATE_DILIGENT_ROOT="$build"
    [[ $mode != native-game ]] || native_game=ON
fi
if [[ ! -d $template ]]; then
    git clone --no-hardlinks "$PS5_REXGLUE_TEMPLATE" "$template"
fi
git -C "$template" cat-file -e c91ff84b7f2cbe3884670fceed8dc924724e04cb^{commit}
if [[ $(git -C "$template" rev-parse HEAD) != c91ff84b7f2cbe3884670fceed8dc924724e04cb ]]; then
    git -C "$template" switch --detach c91ff84b7f2cbe3884670fceed8dc924724e04cb
fi
for dependency in PS5_PayloadSDK PS5_Mesa; do
    if [[ ! -e $build/$dependency ]]; then
        source="$(dirname -- "$PS5_VULKAN_DIR")/$dependency"
        [[ -d $source ]] || fail "Missing $source, needed for package notices"
        ln -s "$source" "$build/$dependency"
    fi
done

smoke=OFF
[[ $mode != smoke ]] || smoke=ON
cmake -S "$template" -B "$title_build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DPS5_VULKAN_DIR="$PS5_VULKAN_DIR" \
    -DTITLE_GAME="$title_game" -DHELLS_GATE_SMOKE="$smoke" -DHELLS_GATE_NATIVE_GAME="$native_game"
cmake --build "$title_build" --target link
if [[ $mode == native-smoke || $mode == native-game ]]; then
    art="$title_game/title-art/sce_sys"
    output="$template/dist/PPSA99343/sce_sys"
    icon="$art/icon0.png"
    [[ -f $icon ]] || icon="$root/ps5/sce_sys/icon0.png"
    cp "$icon" "$output/icon0.png"
    for asset in pic0.dds pic1.dds; do
        if [[ -f $art/$asset ]]; then
            cp "$art/$asset" "$output/$asset"
        fi
    done
    if [[ $mode == native-game && -f $title_game/title-art/launcher.png ]]; then
        cp "$title_game/title-art/launcher.png" "$template/dist/PPSA99343/launcher.png"
    fi
fi
