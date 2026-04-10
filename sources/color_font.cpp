#include "color_font.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_STROKER_H
#include <freetype/tttables.h>
#include <hb.h>
#include <hb-ft.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>
#include <SFML/System/Err.hpp>
#include <SFML/System/InputStream.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

namespace
{
    // FreeType callbacks that operate on a sf::InputStream
    unsigned long read(FT_Stream rec, unsigned long offset, unsigned char* buffer, unsigned long count)
    {
        sf::InputStream* stream = static_cast<sf::InputStream*>(rec->descriptor.pointer);
        if (stream->seek(offset))
        {
            if (count > 0)
            {
                auto result = stream->read(reinterpret_cast<char*>(buffer), static_cast<std::size_t>(count));
                return result ? static_cast<unsigned long>(*result) : 0;
            }
            else
                return 0;
        }
        else
            return count > 0 ? 0 : 1;
    }
    void close(FT_Stream)
    {
    }

    template <typename T, typename U>
    inline T reinterpret(const U& input)
    {
        T output;
        std::memcpy(&output, &input, sizeof(U));
        return output;
    }

    uint64_t combine(float outlineThickness, bool bold, uint32_t index)
    {
        return (static_cast<uint64_t>(reinterpret<uint32_t>(outlineThickness)) << 32) | (static_cast<uint64_t>(bold) << 31) | index;
    }
}

using namespace sf;

ColorFont::ColorFont() :
m_library  (nullptr),
m_face     (nullptr),
m_streamRec(nullptr),
m_stroker  (nullptr),
m_refCount (nullptr),
m_isSmooth (true),
m_info     (),
m_hbFont   (nullptr),
m_hbBuffer (nullptr)
{
    #ifdef SFML_SYSTEM_ANDROID
        m_stream = nullptr;
    #endif
}


////////////////////////////////////////////////////////////
ColorFont::ColorFont(const ColorFont& copy) :
m_library    (copy.m_library),
m_face       (copy.m_face),
m_streamRec  (copy.m_streamRec),
m_stroker    (copy.m_stroker),
m_refCount   (copy.m_refCount),
m_isSmooth   (copy.m_isSmooth),
m_info       (copy.m_info),
m_pages      (copy.m_pages),
m_pixelBuffer(copy.m_pixelBuffer),
m_hbFont     (nullptr),
m_hbBuffer   (nullptr)
{
    #ifdef SFML_SYSTEM_ANDROID
        m_stream = nullptr;
    #endif

    if (m_refCount)
        (*m_refCount)++;
}


////////////////////////////////////////////////////////////
ColorFont::~ColorFont()
{
    cleanup();

    #ifdef SFML_SYSTEM_ANDROID
    if (m_stream)
        delete static_cast<priv::ResourceStream*>(m_stream);
    #endif
}


////////////////////////////////////////////////////////////
bool ColorFont::loadFromFile(const std::string& filename)
{
    #ifndef SFML_SYSTEM_ANDROID

    cleanup();
    m_refCount = new int(1);

    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        sf::err() << "Failed to load font \"" << filename << "\" (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    FT_Face face;
    if (FT_New_Face(static_cast<FT_Library>(m_library), filename.c_str(), 0, &face) != 0)
    {
        sf::err() << "Failed to load font \"" << filename << "\" (failed to create the font face)" << std::endl;
        return false;
    }

    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        sf::err() << "Failed to load font \"" << filename << "\" (failed to create the stroker)" << std::endl;
        FT_Done_Face(face);
        return false;
    }

    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        sf::err() << "Failed to load font \"" << filename << "\" (failed to set the Unicode character set)" << std::endl;
        FT_Stroker_Done(stroker);
        FT_Done_Face(face);
        return false;
    }

    m_stroker = stroker;
    m_face = face;

    m_info.family = face->family_name ? face->family_name : std::string();

    return true;

    #else

    if (m_stream)
        delete static_cast<priv::ResourceStream*>(m_stream);

    m_stream = new priv::ResourceStream(filename);
    return loadFromStream(*static_cast<priv::ResourceStream*>(m_stream));

    #endif
}


////////////////////////////////////////////////////////////
bool ColorFont::loadFromMemory(const void* data, std::size_t sizeInBytes)
{
    cleanup();
    m_refCount = new int(1);

    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        sf::err() << "Failed to load font from memory (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    FT_Face face;
    if (FT_New_Memory_Face(static_cast<FT_Library>(m_library), reinterpret_cast<const FT_Byte*>(data), static_cast<FT_Long>(sizeInBytes), 0, &face) != 0)
    {
        sf::err() << "Failed to load font from memory (failed to create the font face)" << std::endl;
        return false;
    }

    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        sf::err() << "Failed to load font from memory (failed to create the stroker)" << std::endl;
        FT_Done_Face(face);
        return false;
    }

    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        sf::err() << "Failed to load font from memory (failed to set the Unicode character set)" << std::endl;
        FT_Stroker_Done(stroker);
        FT_Done_Face(face);
        return false;
    }

    m_stroker = stroker;
    m_face = face;

    m_info.family = face->family_name ? face->family_name : std::string();

    return true;
}


////////////////////////////////////////////////////////////
bool ColorFont::loadFromStream(InputStream& stream)
{
    cleanup();
    m_refCount = new int(1);

    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        sf::err() << "Failed to load font from stream (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    (void)stream.seek(0);

    FT_StreamRec* rec = new FT_StreamRec;
    std::memset(rec, 0, sizeof(*rec));
    rec->base               = nullptr;
    rec->size               = static_cast<unsigned long>(stream.getSize().value_or(0));
    rec->pos                = 0;
    rec->descriptor.pointer = &stream;
    rec->read               = &read;
    rec->close              = &close;

    FT_Open_Args args;
    args.flags  = FT_OPEN_STREAM;
    args.stream = rec;
    args.driver = 0;

    FT_Face face;
    if (FT_Open_Face(static_cast<FT_Library>(m_library), &args, 0, &face) != 0)
    {
        sf::err() << "Failed to load font from stream (failed to create the font face)" << std::endl;
        delete rec;
        return false;
    }

    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        sf::err() << "Failed to load font from stream (failed to create the stroker)" << std::endl;
        FT_Done_Face(face);
        delete rec;
        return false;
    }

    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        sf::err() << "Failed to load font from stream (failed to set the Unicode character set)" << std::endl;
        FT_Done_Face(face);
        FT_Stroker_Done(stroker);
        delete rec;
        return false;
    }

    m_stroker = stroker;
    m_face = face;
    m_streamRec = rec;

    m_info.family = face->family_name ? face->family_name : std::string();

    return true;
}


////////////////////////////////////////////////////////////
const ColorFont::Info& ColorFont::getInfo() const
{
    return m_info;
}


////////////////////////////////////////////////////////////
const Glyph& ColorFont::getGlyph(uint32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness) const
{
    GlyphTable& glyphs = loadPage(characterSize).glyphs;

    uint64_t key = combine(outlineThickness, bold, FT_Get_Char_Index(static_cast<FT_Face>(m_face), codePoint));

    GlyphTable::const_iterator it = glyphs.find(key);
    if (it != glyphs.end())
    {
        return it->second;
    }
    else
    {
        Glyph glyph = loadGlyph(codePoint, characterSize, bold, outlineThickness);
        return glyphs.insert(std::make_pair(key, glyph)).first->second;
    }
}


////////////////////////////////////////////////////////////
const Glyph& ColorFont::getGlyphByIndex(uint32_t glyphIndex, unsigned int characterSize, bool bold, float outlineThickness) const
{
    GlyphTable& glyphs = loadPage(characterSize).glyphs;

    // Use a distinct key space by setting the high bit of bold field to distinguish from codepoint-based keys
    uint64_t key = combine(outlineThickness, bold, glyphIndex);

    GlyphTable::const_iterator it = glyphs.find(key);
    if (it != glyphs.end())
    {
        return it->second;
    }
    else
    {
        Glyph glyph = loadGlyphByIndex(glyphIndex, characterSize, bold, outlineThickness);
        return glyphs.insert(std::make_pair(key, glyph)).first->second;
    }
}


////////////////////////////////////////////////////////////
bool ColorFont::hasGlyph(uint32_t codePoint) const
{
    return FT_Get_Char_Index(static_cast<FT_Face>(m_face), codePoint) != 0;
}

////////////////////////////////////////////////////////////
float ColorFont::getAscent(unsigned int characterSize) const
{
    FT_Face face = static_cast<FT_Face>(m_face);

    if (setCurrentSize(characterSize))
    {
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(face->size->metrics.ascender) / static_cast<float>(1 << 6);

        return static_cast<float>(FT_MulFix(face->ascender, face->size->metrics.y_scale)) / static_cast<float>(1 << 6);
    }
    else
    {
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
float ColorFont::getDescent(unsigned int characterSize) const
{
    FT_Face face = static_cast<FT_Face>(m_face);

    if (setCurrentSize(characterSize))
    {
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(-face->size->metrics.descender) / static_cast<float>(1 << 6);

        return static_cast<float>(FT_MulFix(-face->descender, face->size->metrics.y_scale)) / static_cast<float>(1 << 6);
    }
    else
    {
        return 0.f;
    }
}



////////////////////////////////////////////////////////////
float ColorFont::getKerning(uint32_t first, uint32_t second, unsigned int characterSize, bool bold) const
{
    if (first == 0 || second == 0)
        return 0.f;

    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && setCurrentSize(characterSize))
    {
        FT_UInt index1 = FT_Get_Char_Index(face, first);
        FT_UInt index2 = FT_Get_Char_Index(face, second);

        float firstRsbDelta  = static_cast<float>(getGlyph(first,  characterSize, bold).rsbDelta);
        float secondLsbDelta = static_cast<float>(getGlyph(second, characterSize, bold).lsbDelta);

        FT_Vector kerning;
        kerning.x = kerning.y = 0;
        if (FT_HAS_KERNING(face))
            FT_Get_Kerning(face, index1, index2, FT_KERNING_UNFITTED, &kerning);

        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(kerning.x);

        return std::floor((secondLsbDelta - firstRsbDelta + static_cast<float>(kerning.x) + 32) / static_cast<float>(1 << 6));
    }
    else
    {
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
float ColorFont::getLineSpacing(unsigned int characterSize) const
{
    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && setCurrentSize(characterSize))
    {
        return static_cast<float>(face->size->metrics.height) / static_cast<float>(1 << 6);
    }
    else
    {
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
float ColorFont::getUnderlinePosition(unsigned int characterSize) const
{
    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && setCurrentSize(characterSize))
    {
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(characterSize) / 10.f;

        return -static_cast<float>(FT_MulFix(face->underline_position, face->size->metrics.y_scale)) / static_cast<float>(1 << 6);
    }
    else
    {
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
float ColorFont::getUnderlineThickness(unsigned int characterSize) const
{
    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && setCurrentSize(characterSize))
    {
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(characterSize) / 14.f;

        return static_cast<float>(FT_MulFix(face->underline_thickness, face->size->metrics.y_scale)) / static_cast<float>(1 << 6);
    }
    else
    {
        return 0.f;
    }
}


////////////////////////////////////////////////////////////
const Texture& ColorFont::getTexture(unsigned int characterSize) const
{
    return loadPage(characterSize).texture;
}

////////////////////////////////////////////////////////////
void ColorFont::setSmooth(bool smooth)
{
    if (smooth != m_isSmooth)
    {
        m_isSmooth = smooth;

        for (PageTable::iterator page = m_pages.begin(); page != m_pages.end(); ++page)
        {
            page->second.texture.setSmooth(m_isSmooth);
        }
    }
}

////////////////////////////////////////////////////////////
bool ColorFont::isSmooth() const
{
    return m_isSmooth;
}

bool ColorFont::isColorEmojiFont() const
{
    FT_Face face = static_cast<FT_Face>(m_face);
    if (!face)
        return false;

    return FT_HAS_COLOR(face);
}

void* ColorFont::getFaceHandle() const
{
    return m_face;
}

void* ColorFont::getHarfBuzzFont() const
{
    if (!m_hbFont && m_face)
    {
        auto* hbFont = hb_ft_font_create(static_cast<FT_Face>(m_face), nullptr);
        hb_ft_font_set_funcs(hbFont);
        m_hbFont = hbFont;
    }
    return m_hbFont;
}

void* ColorFont::getHarfBuzzBuffer() const
{
    if (!m_hbBuffer)
    {
        m_hbBuffer = hb_buffer_create();
    }
    return m_hbBuffer;
}


////////////////////////////////////////////////////////////
ColorFont& ColorFont::operator =(const ColorFont& right)
{
    ColorFont temp(right);

    std::swap(m_library,     temp.m_library);
    std::swap(m_face,        temp.m_face);
    std::swap(m_streamRec,   temp.m_streamRec);
    std::swap(m_stroker,     temp.m_stroker);
    std::swap(m_refCount,    temp.m_refCount);
    std::swap(m_isSmooth,    temp.m_isSmooth);
    std::swap(m_info,        temp.m_info);
    std::swap(m_pages,       temp.m_pages);
    std::swap(m_pixelBuffer, temp.m_pixelBuffer);
    std::swap(m_hbFont,      temp.m_hbFont);
    std::swap(m_hbBuffer,    temp.m_hbBuffer);

    #ifdef SFML_SYSTEM_ANDROID
        std::swap(m_stream, temp.m_stream);
    #endif

    return *this;
}


////////////////////////////////////////////////////////////
void ColorFont::cleanup()
{
    if (m_hbBuffer)
    {
        hb_buffer_destroy(static_cast<hb_buffer_t*>(m_hbBuffer));
        m_hbBuffer = nullptr;
    }

    if (m_hbFont)
    {
        hb_font_destroy(static_cast<hb_font_t*>(m_hbFont));
        m_hbFont = nullptr;
    }

    if (m_refCount)
    {
        (*m_refCount)--;

        if (*m_refCount == 0)
        {
            delete m_refCount;

            if (m_stroker)
                FT_Stroker_Done(static_cast<FT_Stroker>(m_stroker));

            if (m_face)
                FT_Done_Face(static_cast<FT_Face>(m_face));

            if (m_streamRec)
                delete static_cast<FT_StreamRec*>(m_streamRec);

            if (m_library)
                FT_Done_FreeType(static_cast<FT_Library>(m_library));
        }
    }

    m_library   = nullptr;
    m_face      = nullptr;
    m_stroker   = nullptr;
    m_streamRec = nullptr;
    m_refCount  = nullptr;
    m_pages.clear();
    std::vector<uint8_t>().swap(m_pixelBuffer);
}


////////////////////////////////////////////////////////////
ColorFont::Page& ColorFont::loadPage(unsigned int characterSize) const
{
    PageTable::iterator pageIterator = m_pages.find(characterSize);
    if (pageIterator == m_pages.end())
        pageIterator = m_pages.insert(std::make_pair(characterSize, Page(m_isSmooth))).first;

    return pageIterator->second;
}

static sf::Image ScaleImage(const sf::Image& sourceImage, float scaleFactor)
{
    sf::Vector2u originalSize = sourceImage.getSize();
    unsigned int newWidth  = static_cast<unsigned int>(std::round(originalSize.x * scaleFactor));
    unsigned int newHeight = static_cast<unsigned int>(std::round(originalSize.y * scaleFactor));

    sf::Image scaledImage(sf::Vector2u{newWidth, newHeight});

    for (unsigned int y = 0; y < newHeight; ++y)
    {
        for (unsigned int x = 0; x < newWidth; ++x)
        {
            float gx  = ((float)x / newWidth)  * (originalSize.x - 1);
            float gy  = ((float)y / newHeight) * (originalSize.y - 1);
            unsigned int gxi = static_cast<unsigned int>(gx);
            unsigned int gyi = static_cast<unsigned int>(gy);

            sf::Color c00 = sourceImage.getPixel({gxi,     gyi});
            sf::Color c10 = sourceImage.getPixel({gxi + 1 >= originalSize.x ? gxi : gxi + 1, gyi});
            sf::Color c01 = sourceImage.getPixel({gxi,     gyi + 1 >= originalSize.y ? gyi : gyi + 1});
            sf::Color c11 = sourceImage.getPixel({gxi + 1 >= originalSize.x ? gxi : gxi + 1,
                                                  gyi + 1 >= originalSize.y ? gyi : gyi + 1});

            float dx = gx - gxi;
            float dy = gy - gyi;
            sf::Color result(
                static_cast<uint8_t>((c00.r * (1 - dx) + c10.r * dx) * (1 - dy) + (c01.r * (1 - dx) + c11.r * dx) * dy),
                static_cast<uint8_t>((c00.g * (1 - dx) + c10.g * dx) * (1 - dy) + (c01.g * (1 - dx) + c11.g * dx) * dy),
                static_cast<uint8_t>((c00.b * (1 - dx) + c10.b * dx) * (1 - dy) + (c01.b * (1 - dx) + c11.b * dx) * dy),
                static_cast<uint8_t>((c00.a * (1 - dx) + c10.a * dx) * (1 - dy) + (c01.a * (1 - dx) + c11.a * dx) * dy)
            );

            scaledImage.setPixel({x, y}, result);
        }
    }

    return scaledImage;
}

////////////////////////////////////////////////////////////
Glyph ColorFont::loadGlyph(uint32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness) const
{
    Glyph glyph;

    FT_Face face = static_cast<FT_Face>(m_face);
    if (!face)
        return glyph;

    int renderedSize = setCurrentSize(characterSize);
    if (renderedSize == 0)
    {
        sf::err() << "Can't set size for char: " << codePoint << '\n';
        return glyph;
    }

    FT_Int32 flags = (FT_HAS_COLOR(face) ? FT_LOAD_COLOR : FT_LOAD_TARGET_NORMAL) | FT_LOAD_FORCE_AUTOHINT;

    if (outlineThickness != 0)
        flags |= FT_LOAD_NO_BITMAP;
    if (FT_Load_Char(face, codePoint, flags) != 0)
    {
        sf::err() << "Can't load char: " << codePoint << std::endl;
        return glyph;
    }

    FT_Glyph glyphDesc;
    if (FT_Get_Glyph(face->glyph, &glyphDesc) != 0)
    {
        sf::err() << "Can't get glyph for char: " << codePoint << std::endl;
        return glyph;
    }

    FT_Pos weight  = 1 << 6;
    bool   outline = (glyphDesc->format == FT_GLYPH_FORMAT_OUTLINE);
    if (outline)
    {
        if (bold)
        {
            FT_OutlineGlyph outlineGlyph = reinterpret_cast<FT_OutlineGlyph>(glyphDesc);
            FT_Outline_Embolden(&outlineGlyph->outline, weight);
        }

        if (outlineThickness != 0)
        {
            FT_Stroker stroker = static_cast<FT_Stroker>(m_stroker);
            FT_Stroker_Set(stroker, static_cast<FT_Fixed>(outlineThickness * static_cast<float>(1 << 6)), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
            FT_Glyph_Stroke(&glyphDesc, stroker, true);
        }
    }

    auto ft_err = FT_Glyph_To_Bitmap(&glyphDesc, FT_RENDER_MODE_NORMAL, 0, 1);

    if (ft_err)
    {
        sf::err() << "Can't convert glyph to bitmap for char: " << codePoint << std::endl;
        sf::err() << "Code: " << FT_Error_String(ft_err) << std::endl;
        return glyph;
    }

    FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyphDesc);
    FT_Bitmap&     bitmap      = bitmapGlyph->bitmap;

    if (!outline)
    {
        if (bold)
            FT_Bitmap_Embolden(static_cast<FT_Library>(m_library), &bitmap, weight, weight);

        if (outlineThickness != 0)
            sf::err() << "Failed to outline glyph (no fallback available)" << std::endl;
    }

    auto scaleFactor = characterSize / float(renderedSize);

    glyph.advance = static_cast<float>(bitmapGlyph->root.advance.x >> 16);
    if (bold)
        glyph.advance += static_cast<float>(weight) / static_cast<float>(1 << 6);

    glyph.advance *= scaleFactor;

    glyph.lsbDelta = static_cast<int>(static_cast<float>(face->glyph->lsb_delta) * scaleFactor);
    glyph.rsbDelta = static_cast<int>(static_cast<float>(face->glyph->rsb_delta) * scaleFactor);

    unsigned int width  = static_cast<unsigned int>(static_cast<float>(bitmap.width) * scaleFactor);
    unsigned int height = static_cast<unsigned int>(static_cast<float>(bitmap.rows)  * scaleFactor);

    if ((width > 0) && (height > 0))
    {
        const unsigned int padding = 2;

        width  += 2 * padding;
        height += 2 * padding;

        Page& page = loadPage(characterSize);

        glyph.textureRect = findGlyphRect(page, width, height);

        glyph.textureRect.position.x += static_cast<int>(padding);
        glyph.textureRect.position.y += static_cast<int>(padding);
        glyph.textureRect.size.x     -= static_cast<int>(2 * padding);
        glyph.textureRect.size.y     -= static_cast<int>(2 * padding);

        glyph.bounds.position.x =  static_cast<float>(bitmapGlyph->left) * scaleFactor;
        glyph.bounds.position.y = -static_cast<float>(bitmapGlyph->top)  * scaleFactor;
        glyph.bounds.size.x     =  static_cast<float>(bitmap.width)      * scaleFactor;
        glyph.bounds.size.y     =  static_cast<float>(bitmap.rows)       * scaleFactor;

        m_pixelBuffer.resize(width * height * 4);

        uint8_t* current = m_pixelBuffer.data();
        uint8_t* end     = current + width * height * 4;

        while (current != end)
        {
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 0;
        }

        const uint8_t* pixels = bitmap.buffer;
        if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
        {
            for (unsigned int y = padding; y < height - padding; ++y)
            {
                for (unsigned int x = padding; x < width - padding; ++x)
                {
                    std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = ((pixels[(x - padding) / 8]) & (1 << (7 - ((x - padding) % 8)))) ? 255 : 0;
                }
                pixels += bitmap.pitch;
            }
        }
        else if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
        {
            sf::Image emoji(sf::Vector2u{bitmap.width, bitmap.rows});

            for (unsigned int y = 0; y < bitmap.rows; ++y)
            {
                for (unsigned int x = 0; x < bitmap.width; ++x)
                {
                    std::size_t sourceIndex = x * 4;
                    emoji.setPixel({x, y}, sf::Color{
                        pixels[sourceIndex + 2],
                        pixels[sourceIndex + 1],
                        pixels[sourceIndex + 0],
                        pixels[sourceIndex + 3]
                    });
                }
                pixels += bitmap.pitch;
            }

            if (renderedSize != characterSize)
                emoji = ScaleImage(emoji, scaleFactor);

            for (unsigned int y = 0; y < emoji.getSize().y; ++y)
            {
                for (unsigned int x = 0; x < emoji.getSize().x; ++x)
                {
                    auto pixel = emoji.getPixel({x, y});
                    auto index = (padding + y) * width + padding + x;

                    m_pixelBuffer[index * 4 + 0] = pixel.r;
                    m_pixelBuffer[index * 4 + 1] = pixel.g;
                    m_pixelBuffer[index * 4 + 2] = pixel.b;
                    m_pixelBuffer[index * 4 + 3] = pixel.a;
                }
            }
        }
        else
        {
            for (unsigned int y = padding; y < height - padding; ++y)
            {
                for (unsigned int x = padding; x < width - padding; ++x)
                {
                    std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = pixels[x - padding];
                }
                pixels += bitmap.pitch;
            }
        }

        unsigned int x = static_cast<unsigned int>(glyph.textureRect.position.x) - padding;
        unsigned int y = static_cast<unsigned int>(glyph.textureRect.position.y) - padding;
        unsigned int w = static_cast<unsigned int>(glyph.textureRect.size.x) + 2 * padding;
        unsigned int h = static_cast<unsigned int>(glyph.textureRect.size.y) + 2 * padding;
        page.texture.update(m_pixelBuffer.data(), sf::Vector2u{w, h}, sf::Vector2u{x, y});
    }

    FT_Done_Glyph(glyphDesc);

    return glyph;
}


////////////////////////////////////////////////////////////
Glyph ColorFont::loadGlyphByIndex(uint32_t glyphIndex, unsigned int characterSize, bool bold, float outlineThickness) const
{
    Glyph glyph;

    FT_Face face = static_cast<FT_Face>(m_face);
    if (!face)
        return glyph;

    int renderedSize = setCurrentSize(characterSize);
    if (renderedSize == 0)
        return glyph;

    FT_Int32 flags = (FT_HAS_COLOR(face) ? FT_LOAD_COLOR : FT_LOAD_TARGET_NORMAL) | FT_LOAD_FORCE_AUTOHINT;

    if (outlineThickness != 0)
        flags |= FT_LOAD_NO_BITMAP;
    if (FT_Load_Glyph(face, glyphIndex, flags) != 0)
        return glyph;

    FT_Glyph glyphDesc;
    if (FT_Get_Glyph(face->glyph, &glyphDesc) != 0)
        return glyph;

    FT_Pos weight  = 1 << 6;
    bool   outline = (glyphDesc->format == FT_GLYPH_FORMAT_OUTLINE);
    if (outline)
    {
        if (bold)
        {
            FT_OutlineGlyph outlineGlyph = reinterpret_cast<FT_OutlineGlyph>(glyphDesc);
            FT_Outline_Embolden(&outlineGlyph->outline, weight);
        }

        if (outlineThickness != 0)
        {
            FT_Stroker stroker = static_cast<FT_Stroker>(m_stroker);
            FT_Stroker_Set(stroker, static_cast<FT_Fixed>(outlineThickness * static_cast<float>(1 << 6)), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
            FT_Glyph_Stroke(&glyphDesc, stroker, true);
        }
    }

    if (FT_Glyph_To_Bitmap(&glyphDesc, FT_RENDER_MODE_NORMAL, 0, 1))
    {
        FT_Done_Glyph(glyphDesc);
        return glyph;
    }

    FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyphDesc);
    FT_Bitmap&     bitmap      = bitmapGlyph->bitmap;

    if (!outline)
    {
        if (bold)
            FT_Bitmap_Embolden(static_cast<FT_Library>(m_library), &bitmap, weight, weight);
    }

    auto scaleFactor = characterSize / float(renderedSize);

    glyph.advance = static_cast<float>(bitmapGlyph->root.advance.x >> 16);
    if (bold)
        glyph.advance += static_cast<float>(weight) / static_cast<float>(1 << 6);

    glyph.advance *= scaleFactor;

    glyph.lsbDelta = static_cast<int>(static_cast<float>(face->glyph->lsb_delta) * scaleFactor);
    glyph.rsbDelta = static_cast<int>(static_cast<float>(face->glyph->rsb_delta) * scaleFactor);

    unsigned int width  = static_cast<unsigned int>(static_cast<float>(bitmap.width) * scaleFactor);
    unsigned int height = static_cast<unsigned int>(static_cast<float>(bitmap.rows)  * scaleFactor);

    if ((width > 0) && (height > 0))
    {
        const unsigned int padding = 2;

        width  += 2 * padding;
        height += 2 * padding;

        Page& page = loadPage(characterSize);

        glyph.textureRect = findGlyphRect(page, width, height);

        glyph.textureRect.position.x += static_cast<int>(padding);
        glyph.textureRect.position.y += static_cast<int>(padding);
        glyph.textureRect.size.x     -= static_cast<int>(2 * padding);
        glyph.textureRect.size.y     -= static_cast<int>(2 * padding);

        glyph.bounds.position.x =  static_cast<float>(bitmapGlyph->left) * scaleFactor;
        glyph.bounds.position.y = -static_cast<float>(bitmapGlyph->top)  * scaleFactor;
        glyph.bounds.size.x     =  static_cast<float>(bitmap.width)      * scaleFactor;
        glyph.bounds.size.y     =  static_cast<float>(bitmap.rows)       * scaleFactor;

        m_pixelBuffer.resize(width * height * 4);

        uint8_t* current = m_pixelBuffer.data();
        uint8_t* end     = current + width * height * 4;

        while (current != end)
        {
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 0;
        }

        const uint8_t* pixels = bitmap.buffer;
        if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
        {
            for (unsigned int y = padding; y < height - padding; ++y)
            {
                for (unsigned int x = padding; x < width - padding; ++x)
                {
                    std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = ((pixels[(x - padding) / 8]) & (1 << (7 - ((x - padding) % 8)))) ? 255 : 0;
                }
                pixels += bitmap.pitch;
            }
        }
        else if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
        {
            sf::Image emoji(sf::Vector2u{bitmap.width, bitmap.rows});

            for (unsigned int y = 0; y < bitmap.rows; ++y)
            {
                for (unsigned int x = 0; x < bitmap.width; ++x)
                {
                    std::size_t sourceIndex = x * 4;
                    emoji.setPixel({x, y}, sf::Color{
                        pixels[sourceIndex + 2],
                        pixels[sourceIndex + 1],
                        pixels[sourceIndex + 0],
                        pixels[sourceIndex + 3]
                    });
                }
                pixels += bitmap.pitch;
            }

            if (renderedSize != characterSize)
                emoji = ScaleImage(emoji, scaleFactor);

            for (unsigned int y = 0; y < emoji.getSize().y; ++y)
            {
                for (unsigned int x = 0; x < emoji.getSize().x; ++x)
                {
                    auto pixel = emoji.getPixel({x, y});
                    auto index = (padding + y) * width + padding + x;

                    m_pixelBuffer[index * 4 + 0] = pixel.r;
                    m_pixelBuffer[index * 4 + 1] = pixel.g;
                    m_pixelBuffer[index * 4 + 2] = pixel.b;
                    m_pixelBuffer[index * 4 + 3] = pixel.a;
                }
            }
        }
        else
        {
            for (unsigned int y = padding; y < height - padding; ++y)
            {
                for (unsigned int x = padding; x < width - padding; ++x)
                {
                    std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = pixels[x - padding];
                }
                pixels += bitmap.pitch;
            }
        }

        unsigned int x = static_cast<unsigned int>(glyph.textureRect.position.x) - padding;
        unsigned int y = static_cast<unsigned int>(glyph.textureRect.position.y) - padding;
        unsigned int w = static_cast<unsigned int>(glyph.textureRect.size.x) + 2 * padding;
        unsigned int h = static_cast<unsigned int>(glyph.textureRect.size.y) + 2 * padding;
        page.texture.update(m_pixelBuffer.data(), sf::Vector2u{w, h}, sf::Vector2u{x, y});
    }

    FT_Done_Glyph(glyphDesc);

    return glyph;
}


////////////////////////////////////////////////////////////
IntRect ColorFont::findGlyphRect(Page& page, unsigned int width, unsigned int height) const
{
    Row* row = nullptr;
    float bestRatio = 0;
    for (std::vector<Row>::iterator it = page.rows.begin(); it != page.rows.end() && !row; ++it)
    {
        float ratio = static_cast<float>(height) / static_cast<float>(it->height);

        if ((ratio < 0.7f) || (ratio > 1.f))
            continue;

        if (width > page.texture.getSize().x - it->width)
            continue;

        if (ratio < bestRatio)
            continue;

        row = &*it;
        bestRatio = ratio;
    }

    if (!row)
    {
        unsigned int rowHeight = height + height / 10;
        while ((page.nextRow + rowHeight >= page.texture.getSize().y) || (width >= page.texture.getSize().x))
        {
            unsigned int textureWidth  = page.texture.getSize().x;
            unsigned int textureHeight = page.texture.getSize().y;
            if ((textureWidth * 2 <= Texture::getMaximumSize()) && (textureHeight * 2 <= Texture::getMaximumSize()))
            {
                Texture newTexture(sf::Vector2u{textureWidth * 2, textureHeight * 2});
                newTexture.setSmooth(m_isSmooth);
                newTexture.update(page.texture);
                page.texture = std::move(newTexture);
            }
            else
            {
                sf::err() << "Failed to add a new character to the font: the maximum texture size has been reached" << std::endl;
                return IntRect({0, 0}, {2, 2});
            }
        }

        page.rows.push_back(Row(page.nextRow, rowHeight));
        page.nextRow += rowHeight;
        row = &page.rows.back();
    }

    IntRect rect(sf::Vector2i{static_cast<int>(row->width), static_cast<int>(row->top)},
                 sf::Vector2i{static_cast<int>(width),      static_cast<int>(height)});

    row->width += width;

    return rect;
}


////////////////////////////////////////////////////////////
int ColorFont::setCurrentSize(unsigned int characterSize) const
{
    FT_Face   face        = static_cast<FT_Face>(m_face);
    FT_UShort currentSize = face->size->metrics.x_ppem;

    if (currentSize != characterSize)
    {
        if (FT_HAS_COLOR(face) && face->available_sizes)
        {
            int best_match = 0;
            int diff = std::labs(characterSize - face->available_sizes[0].width);
            for (int i = 1; i < face->num_fixed_sizes; ++i)
            {
                int ndiff = std::labs(characterSize - face->available_sizes[i].width);
                if (ndiff < diff)
                {
                    best_match = i;
                    diff = ndiff;
                }
            }
            FT_Error result = FT_Select_Size(face, best_match);
            return result == FT_Err_Ok ? face->available_sizes[best_match].height : 0;
        }

        FT_Error result = FT_Set_Pixel_Sizes(face, 0, characterSize);

        if (result == FT_Err_Invalid_Pixel_Size)
        {
            if (!FT_IS_SCALABLE(face))
            {
                sf::err() << "Failed to set bitmap font size to " << characterSize << std::endl;
                sf::err() << "Available sizes are: ";
                for (int i = 0; i < face->num_fixed_sizes; ++i)
                {
                    const long size = (face->available_sizes[i].y_ppem + 32) >> 6;
                    sf::err() << size << " ";
                }
                sf::err() << std::endl;
            }
            else
            {
                sf::err() << "Failed to set font size to " << characterSize << std::endl;
            }
        }
        return result == FT_Err_Ok ? characterSize : 0;
    }

    return characterSize;
}

ColorFont::Page::Page(bool smooth) :
    nextRow(3)
{
    sf::Image image(sf::Vector2u{128, 128}, sf::Color(255, 255, 255, 0));

    for (unsigned int x = 0; x < 2; ++x)
        for (unsigned int y = 0; y < 2; ++y)
            image.setPixel({x, y}, sf::Color(255, 255, 255, 255));

    (void)texture.loadFromImage(image);
    texture.setSmooth(smooth);
}
