#include "color_shaped_string.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

ColorShapedString ColorShapedString::shape(const ColorFont& font, const sf::String& string)
{
    ColorShapedString result;
    result.m_source = string;
    result.m_font = &font;

    if (string.isEmpty())
        return result;

    FT_Face ftFace = static_cast<FT_Face>(font.getFaceHandle());
    if (!ftFace)
        return result;

    // Ensure the font size is set by triggering a glyph load
    font.getGlyph(L' ', 30, false);

    hb_font_t* hbFont = static_cast<hb_font_t*>(font.getHarfBuzzFont());
    hb_buffer_t* hbBuffer = static_cast<hb_buffer_t*>(font.getHarfBuzzBuffer());
    if (!hbFont || !hbBuffer)
        return result;

    FT_Int32 loadFlags = (FT_HAS_COLOR(ftFace) ? FT_LOAD_COLOR : FT_LOAD_TARGET_NORMAL) | FT_LOAD_FORCE_AUTOHINT;
    hb_ft_font_set_load_flags(hbFont, loadFlags);

    hb_buffer_clear_contents(hbBuffer);
    hb_buffer_set_content_type(hbBuffer, HB_BUFFER_CONTENT_TYPE_UNICODE);
    hb_buffer_set_direction(hbBuffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(hbBuffer, HB_SCRIPT_COMMON);
    hb_buffer_set_cluster_level(hbBuffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);

    for (std::size_t i = 0; i < string.getSize(); ++i)
        hb_buffer_add(hbBuffer, string[i], static_cast<unsigned int>(i));

    hb_buffer_guess_segment_properties(hbBuffer);
    hb_shape(hbFont, hbBuffer, nullptr, 0);

    unsigned int glyphCount = hb_buffer_get_length(hbBuffer);
    const hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, nullptr);

    result.m_glyphs.reserve(glyphCount);
    for (unsigned int i = 0; i < glyphCount; ++i)
    {
        ShapedGlyph sg;
        sg.glyphIndex = glyphInfo[i].codepoint; // after shaping, this is a glyph index
        sg.cluster = glyphInfo[i].cluster;
        result.m_glyphs.push_back(sg);
    }

    return result;
}

uint32_t ColorShapedString::getSourceCodepoint(std::size_t glyphIdx) const
{
    if (glyphIdx >= m_glyphs.size())
        return 0;

    uint32_t cluster = m_glyphs[glyphIdx].cluster;
    if (cluster < m_source.getSize())
        return m_source[cluster];

    return 0;
}

uint32_t ColorShapedString::getFirstGlyphIndex() const
{
    if (m_glyphs.empty())
        return 0;

    return m_glyphs[0].glyphIndex;
}
