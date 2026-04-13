#pragma once

#include "color_font.hpp"
#include <SFML/System/String.hpp>
#include <vector>
#include <cstdint>

struct ShapedGlyph {
    uint32_t glyphIndex = 0;
    uint32_t cluster = 0;
};

class ColorShapedString {
public:
    ColorShapedString() = default;

    static ColorShapedString shape(const ColorFont& font, const sf::String& string);

    const std::vector<ShapedGlyph>& getGlyphs() const { return m_glyphs; }
    const sf::String& getSource() const { return m_source; }
    const ColorFont* getFont() const { return m_font; }
    bool isEmpty() const { return m_glyphs.empty(); }
    std::size_t getGlyphCount() const { return m_glyphs.size(); }

    uint32_t getSourceCodepoint(std::size_t glyphIdx) const;

    uint32_t getFirstGlyphIndex() const;

private:
    std::vector<ShapedGlyph> m_glyphs;
    sf::String m_source;
    const ColorFont* m_font = nullptr;
};
