#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Glyph.hpp>
#include <cstdint>
#include <map>
#include <vector>
#include <string>

class ColorFont
{
public:

    struct Info
    {
        std::string family;
    };

public:

    ColorFont();

    ColorFont(const ColorFont& copy);

    ~ColorFont();

    bool loadFromFile(const std::string& filename);

    bool loadFromMemory(const void* data, std::size_t sizeInBytes);

    bool loadFromStream(sf::InputStream& stream);

    const Info& getInfo() const;

    const sf::Glyph& getGlyph(uint32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness = 0) const;

    const sf::Glyph& getGlyphByIndex(uint32_t glyphIndex, unsigned int characterSize, bool bold, float outlineThickness = 0) const;

    bool hasGlyph(uint32_t codePoint) const;

    void* getFaceHandle() const;

    float getAscent(unsigned int characterSize) const;

    float getDescent(unsigned int characterSize) const;

    float getKerning(uint32_t first, uint32_t second, unsigned int characterSize, bool bold = false) const;

    float getLineSpacing(unsigned int characterSize) const;

    float getUnderlinePosition(unsigned int characterSize) const;

    float getUnderlineThickness(unsigned int characterSize) const;

    const sf::Texture& getTexture(unsigned int characterSize) const;

    void setSmooth(bool smooth);

    bool isSmooth() const;

    bool isColorEmojiFont() const;

    ColorFont& operator =(const ColorFont& right);

private:

    struct Row
    {
        Row(unsigned int rowTop, unsigned int rowHeight) : width(0), top(rowTop), height(rowHeight) {}

        unsigned int width;
        unsigned int top;
        unsigned int height;
    };

    typedef std::map<uint64_t, sf::Glyph> GlyphTable;

    struct Page
    {
        explicit Page(bool smooth);

        GlyphTable       glyphs;
        sf::Texture      texture;
        unsigned int     nextRow;
        std::vector<Row> rows;
    };

    void cleanup();

    Page& loadPage(unsigned int characterSize) const;

    sf::Glyph loadGlyph(uint32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness) const;

    sf::Glyph loadGlyphByIndex(uint32_t glyphIndex, unsigned int characterSize, bool bold, float outlineThickness) const;

    sf::IntRect findGlyphRect(Page& page, unsigned int width, unsigned int height) const;

    int setCurrentSize(unsigned int characterSize) const;

    typedef std::map<unsigned int, Page> PageTable;

    void*                   m_library;
    void*                   m_face;
    void*                   m_streamRec;
    void*                   m_stroker;
    int*                    m_refCount;
    bool                    m_isSmooth;
    Info                    m_info;
    mutable PageTable       m_pages;
    mutable std::vector<uint8_t> m_pixelBuffer;
    #ifdef SFML_SYSTEM_ANDROID
    void*                   m_stream;
    #endif
};
