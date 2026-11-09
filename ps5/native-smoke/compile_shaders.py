"""Compile the native renderer's fixed GLSL passes on the build host."""

import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def literal(path, name):
    text = path.read_text(encoding="utf-8")
    match = re.search(r'\b' + re.escape(name) + r'\s*(?:\[\])?\s*=\s*R"\((.*?)\)"',
                      text, re.DOTALL)
    if not match:
        raise ValueError(f"{name} was not found in {path}")
    return match[1].encode("utf-8")


def hash_source(source):
    value = 14695981039346656037
    for byte in source:
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def main(root, validator, output):
    renderer = root / "src/renderer"
    passes = {
        "vulkan_device.cpp": {
            "kBlitVS": "vert", "kBlitPS": "frag", "kFullscreenVS": "vert",
        },
        "renderer.cpp": {"kDepthCopyPS": "frag", "kDepthBitsPS": "frag", "kReplayPS": "frag"},
        "ui_overlay.cpp": {"kOverlayVS": "vert", "kOverlayPS": "frag"},
        "smaa_pass.cpp": {"kEdgesPS": "frag", "kWeightsPS": "frag", "kBlendPS": "frag"},
    }
    smaa = literal(renderer / "smaa_pass.cpp", "kPrelude")
    smaa += (root / "thirdparty/smaa/SMAA.hlsl").read_bytes()
    lines = ["#pragma once", "#include <cstdint>", "#include <span>", "#include <string_view>",
             "", "namespace renderer {", "namespace {"]
    cases = []
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        for filename, entries in passes.items():
            for name, stage in entries.items():
                source = literal(renderer / filename, name)
                if filename == "smaa_pass.cpp":
                    source = smaa + source
                shader = temp / f"{name}.{stage}"
                spirv = temp / f"{name}.spv"
                shader.write_bytes(source)
                subprocess.run([validator, "-V", "--target-env", "vulkan1.0", "-o",
                                str(spirv), str(shader)], check=True)
                words = list(word[0] for word in struct.iter_unpack("<I", spirv.read_bytes()))
                lines.append(f"constexpr uint32_t kPs5Spirv_{name}[] = {{")
                for i in range(0, len(words), 8):
                    lines.append("  " + ", ".join(f"0x{w:08x}" for w in words[i:i + 8]) + ",")
                lines.append("};")
                cases.append(f"    case 0x{hash_source(source):016x}ULL: return kPs5Spirv_{name};")
    lines += [
        "std::span<const uint32_t> FindPs5Spirv(std::string_view source) {",
        "  uint64_t hash = 14695981039346656037ULL;",
        "  for (unsigned char byte : source) hash = (hash ^ byte) * 1099511628211ULL;",
        "  switch (hash) {",
        *cases,
        "    default: return {};",
        "  }",
        "}",
        "}  // namespace",
        "}  // namespace renderer",
    ]
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main(Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3]))
