function(hells_gate_embed_glyph_icons target asset_dir)
    file(GLOB GLYPH_ICON_FILES CONFIGURE_DEPENDS
        "${asset_dir}/*.png")
    list(SORT GLYPH_ICON_FILES)
    set(GLYPH_ICON_ARRAYS "")
    set(GLYPH_ICON_TABLE "")
    set(glyph_index 0)
    foreach(glyph_icon IN LISTS GLYPH_ICON_FILES)
        get_filename_component(glyph_name "${glyph_icon}" NAME_WE)
        file(READ "${glyph_icon}" glyph_hex HEX)
        file(SIZE "${glyph_icon}" glyph_size)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," glyph_hex "${glyph_hex}")
        string(APPEND GLYPH_ICON_ARRAYS "const unsigned char kIcon${glyph_index}[] = {${glyph_hex}};\n")
        string(APPEND GLYPH_ICON_TABLE "    {\"${glyph_name}\", kIcon${glyph_index}, ${glyph_size}},\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${glyph_icon}")
        math(EXPR glyph_index "${glyph_index} + 1")
    endforeach()
    set(GLYPH_ICON_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated_glyphs/glyph_icons.cpp")
    file(CONFIGURE OUTPUT "${GLYPH_ICON_SOURCE}" @ONLY CONTENT
"#include <cstddef>

namespace renderer {

struct GlyphIconFile {
  const char* name;
  const unsigned char* data;
  size_t size;
};

namespace {
@GLYPH_ICON_ARRAYS@}

extern const GlyphIconFile kGlyphIconFiles[] = {
@GLYPH_ICON_TABLE@};
extern const size_t kGlyphIconFileCount = sizeof(kGlyphIconFiles) / sizeof(kGlyphIconFiles[0]);

}
")
    target_sources(${target} PRIVATE "${GLYPH_ICON_SOURCE}")
endfunction()
