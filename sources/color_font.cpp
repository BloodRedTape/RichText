#include "color_font.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_STROKER_H
#include <freetype2/freetype/tttables.h> 
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <SFML/System/Err.hpp>
#include <SFML/System/InputStream.hpp>

namespace
{
    // FreeType callbacks that operate on a sf::InputStream
    unsigned long read(FT_Stream rec, unsigned long offset, unsigned char* buffer, unsigned long count)
    {
        sf::Int64 convertedOffset = static_cast<sf::Int64>(offset);
        sf::InputStream* stream = static_cast<sf::InputStream*>(rec->descriptor.pointer);
        if (stream->seek(convertedOffset) == convertedOffset)
        {
            if (count > 0)
                return static_cast<unsigned long>(stream->read(reinterpret_cast<char*>(buffer), static_cast<sf::Int64>(count)));
            else
                return 0;
        }
        else
            return count > 0 ? 0 : 1; // error code is 0 if we're reading, or nonzero if we're seeking
    }
    void close(FT_Stream)
    {
    }

    // Helper to intepret memory as a specific type
    template <typename T, typename U>
    inline T reinterpret(const U& input)
    {
        T output;
        std::memcpy(&output, &input, sizeof(U));
        return output;
    }

    // Combine outline thickness, boldness and font glyph index into a single 64-bit key
    sf::Uint64 combine(float outlineThickness, bool bold, sf::Uint32 index)
    {
        return (static_cast<sf::Uint64>(reinterpret<sf::Uint32>(outlineThickness)) << 32) | (static_cast<sf::Uint64>(bold) << 31) | index;
    }
}

using namespace sf;

ColorFont::ColorFont() :
m_library  (NULL),
m_face     (NULL),
m_streamRec(NULL),
m_stroker  (NULL),
m_refCount (NULL),
m_isSmooth (true),
m_info     ()
{
    #ifdef SFML_SYSTEM_ANDROID
        m_stream = NULL;
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
m_pixelBuffer(copy.m_pixelBuffer)
{
    #ifdef SFML_SYSTEM_ANDROID
        m_stream = NULL;
    #endif

    // Note: as FreeType doesn't provide functions for copying/cloning,
    // we must share all the FreeType pointers

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

    // Cleanup the previous resources
    cleanup();
    m_refCount = new int(1);

    // Initialize FreeType
    // Note: we initialize FreeType for every font instance in order to avoid having a single
    // global manager that would create a lot of issues regarding creation and destruction order.
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        err() << "Failed to load font \"" << filename << "\" (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    // Load the new font face from the specified file
    FT_Face face;
    if (FT_New_Face(static_cast<FT_Library>(m_library), filename.c_str(), 0, &face) != 0)
    {
        err() << "Failed to load font \"" << filename << "\" (failed to create the font face)" << std::endl;
        return false;
    }

    // Load the stroker that will be used to outline the font
    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        err() << "Failed to load font \"" << filename << "\" (failed to create the stroker)" << std::endl;
        FT_Done_Face(face);
        return false;
    }

    // Select the unicode character map
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        err() << "Failed to load font \"" << filename << "\" (failed to set the Unicode character set)" << std::endl;
        FT_Stroker_Done(stroker);
        FT_Done_Face(face);
        return false;
    }

    // Store the loaded font in our ugly void* :)
    m_stroker = stroker;
    m_face = face;

    // Store the font information
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
    // Cleanup the previous resources
    cleanup();
    m_refCount = new int(1);

    // Initialize FreeType
    // Note: we initialize FreeType for every font instance in order to avoid having a single
    // global manager that would create a lot of issues regarding creation and destruction order.
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        err() << "Failed to load font from memory (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    // Load the new font face from the specified file
    FT_Face face;
    if (FT_New_Memory_Face(static_cast<FT_Library>(m_library), reinterpret_cast<const FT_Byte*>(data), static_cast<FT_Long>(sizeInBytes), 0, &face) != 0)
    {
        err() << "Failed to load font from memory (failed to create the font face)" << std::endl;
        return false;
    }

    // Load the stroker that will be used to outline the font
    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        err() << "Failed to load font from memory (failed to create the stroker)" << std::endl;
        FT_Done_Face(face);
        return false;
    }

    // Select the Unicode character map
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        err() << "Failed to load font from memory (failed to set the Unicode character set)" << std::endl;
        FT_Stroker_Done(stroker);
        FT_Done_Face(face);
        return false;
    }

    // Store the loaded font in our ugly void* :)
    m_stroker = stroker;
    m_face = face;

    // Store the font information
    m_info.family = face->family_name ? face->family_name : std::string();

    return true;
}


////////////////////////////////////////////////////////////
bool ColorFont::loadFromStream(InputStream& stream)
{
    // Cleanup the previous resources
    cleanup();
    m_refCount = new int(1);

    // Initialize FreeType
    // Note: we initialize FreeType for every font instance in order to avoid having a single
    // global manager that would create a lot of issues regarding creation and destruction order.
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        err() << "Failed to load font from stream (failed to initialize FreeType)" << std::endl;
        return false;
    }
    m_library = library;

    // Make sure that the stream's reading position is at the beginning
    stream.seek(0);

    // Prepare a wrapper for our stream, that we'll pass to FreeType callbacks
    FT_StreamRec* rec = new FT_StreamRec;
    std::memset(rec, 0, sizeof(*rec));
    rec->base               = NULL;
    rec->size               = static_cast<unsigned long>(stream.getSize());
    rec->pos                = 0;
    rec->descriptor.pointer = &stream;
    rec->read               = &read;
    rec->close              = &close;

    // Setup the FreeType callbacks that will read our stream
    FT_Open_Args args;
    args.flags  = FT_OPEN_STREAM;
    args.stream = rec;
    args.driver = 0;

    // Load the new font face from the specified stream
    FT_Face face;
    if (FT_Open_Face(static_cast<FT_Library>(m_library), &args, 0, &face) != 0)
    {
        err() << "Failed to load font from stream (failed to create the font face)" << std::endl;
        delete rec;
        return false;
    }

    // Load the stroker that will be used to outline the font
    FT_Stroker stroker;
    if (FT_Stroker_New(static_cast<FT_Library>(m_library), &stroker) != 0)
    {
        err() << "Failed to load font from stream (failed to create the stroker)" << std::endl;
        FT_Done_Face(face);
        delete rec;
        return false;
    }

    // Select the Unicode character map
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        err() << "Failed to load font from stream (failed to set the Unicode character set)" << std::endl;
        FT_Done_Face(face);
        FT_Stroker_Done(stroker);
        delete rec;
        return false;
    }

    // Store the loaded font in our ugly void* :)
    m_stroker = stroker;
    m_face = face;
    m_streamRec = rec;

    // Store the font information
    m_info.family = face->family_name ? face->family_name : std::string();

    return true;
}


////////////////////////////////////////////////////////////
const sf::Font::Info& ColorFont::getInfo() const
{
    return m_info;
}


////////////////////////////////////////////////////////////
const Glyph& ColorFont::getGlyph(Uint32 codePoint, unsigned int characterSize, bool bold, float outlineThickness) const
{
    // Get the page corresponding to the character size
    GlyphTable& glyphs = loadPage(characterSize).glyphs;

    // Build the key by combining the glyph index (based on code point), bold flag, and outline thickness
    Uint64 key = combine(outlineThickness, bold, FT_Get_Char_Index(static_cast<FT_Face>(m_face), codePoint));

    // Search the glyph into the cache
    GlyphTable::const_iterator it = glyphs.find(key);
    if (it != glyphs.end())
    {
        // Found: just return it
        return it->second;
    }
    else
    {
        // Not found: we have to load it
        Glyph glyph = loadGlyph(codePoint, characterSize, bold, outlineThickness);
        return glyphs.insert(std::make_pair(key, glyph)).first->second;
    }
}


////////////////////////////////////////////////////////////
bool ColorFont::hasGlyph(Uint32 codePoint) const
{
    return FT_Get_Char_Index(static_cast<FT_Face>(m_face), codePoint) != 0;
}


////////////////////////////////////////////////////////////
float ColorFont::getKerning(Uint32 first, Uint32 second, unsigned int characterSize, bool bold) const
{
    // Special case where first or second is 0 (null character)
    if (first == 0 || second == 0)
        return 0.f;

    FT_Face face = static_cast<FT_Face>(m_face);

    if (face && setCurrentSize(characterSize))
    {
        // Convert the characters to indices
        FT_UInt index1 = FT_Get_Char_Index(face, first);
        FT_UInt index2 = FT_Get_Char_Index(face, second);

        // Retrieve position compensation deltas generated by FT_LOAD_FORCE_AUTOHINT flag
        float firstRsbDelta = static_cast<float>(getGlyph(first, characterSize, bold).rsbDelta);
        float secondLsbDelta = static_cast<float>(getGlyph(second, characterSize, bold).lsbDelta);

        // Get the kerning vector if present
        FT_Vector kerning;
        kerning.x = kerning.y = 0;
        if (FT_HAS_KERNING(face))
            FT_Get_Kerning(face, index1, index2, FT_KERNING_UNFITTED, &kerning);

        // X advance is already in pixels for bitmap fonts
        if (!FT_IS_SCALABLE(face))
            return static_cast<float>(kerning.x);

        // Combine kerning with compensation deltas and return the X advance
        // Flooring is required as we use FT_KERNING_UNFITTED flag which is not quantized in 64 based grid
        return std::floor((secondLsbDelta - firstRsbDelta + static_cast<float>(kerning.x) + 32) / static_cast<float>(1 << 6));
    }
    else
    {
        // Invalid font
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
        // Return a fixed position if font is a bitmap font
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
        // Return a fixed thickness if font is a bitmap font
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

    #ifdef SFML_SYSTEM_ANDROID
        std::swap(m_stream, temp.m_stream);
    #endif

    return *this;
}


////////////////////////////////////////////////////////////
void ColorFont::cleanup()
{
    // Check if we must destroy the FreeType pointers
    if (m_refCount)
    {
        // Decrease the reference counter
        (*m_refCount)--;

        // Free the resources only if we are the last owner
        if (*m_refCount == 0)
        {
            // Delete the reference counter
            delete m_refCount;

            // Destroy the stroker
            if (m_stroker)
                FT_Stroker_Done(static_cast<FT_Stroker>(m_stroker));

            // Destroy the font face
            if (m_face)
                FT_Done_Face(static_cast<FT_Face>(m_face));

            // Destroy the stream rec instance, if any (must be done after FT_Done_Face!)
            if (m_streamRec)
                delete static_cast<FT_StreamRec*>(m_streamRec);

            // Close the library
            if (m_library)
                FT_Done_FreeType(static_cast<FT_Library>(m_library));
        }
    }

    // Reset members
    m_library   = NULL;
    m_face      = NULL;
    m_stroker   = NULL;
    m_streamRec = NULL;
    m_refCount  = NULL;
    m_pages.clear();
    std::vector<Uint8>().swap(m_pixelBuffer);
}


////////////////////////////////////////////////////////////
ColorFont::Page& ColorFont::loadPage(unsigned int characterSize) const
{
    // TODO: Remove this method and use try_emplace instead when updating to C++17
    PageTable::iterator pageIterator = m_pages.find(characterSize);
    if (pageIterator == m_pages.end())
        pageIterator = m_pages.insert(std::make_pair(characterSize, Page(m_isSmooth))).first;

    return pageIterator->second;
}

static sf::Image ScaleImage(const sf::Image &sourceImage, float scaleFactor) {
    unsigned int originalWidth = sourceImage.getSize().x;
    unsigned int originalHeight = sourceImage.getSize().y;
    unsigned int newWidth = static_cast<unsigned int>(std::floor(originalWidth * scaleFactor));
    unsigned int newHeight = static_cast<unsigned int>(std::floor(originalHeight * scaleFactor));
    
    sf::Image scaledImage;
    scaledImage.create(newWidth, newHeight);

    for (unsigned int y = 0; y < newHeight; ++y) {
        for (unsigned int x = 0; x < newWidth; ++x) {
            float gx = ((float)x / newWidth) * (originalWidth - 1);
            float gy = ((float)y / newHeight) * (originalHeight - 1);
            unsigned int gxi = static_cast<unsigned int>(gx);
            unsigned int gyi = static_cast<unsigned int>(gy);
            sf::Color c00 = sourceImage.getPixel(gxi, gyi);
            sf::Color c10 = sourceImage.getPixel(gxi + 1 >= originalWidth ? gxi : gxi + 1, gyi);
            sf::Color c01 = sourceImage.getPixel(gxi, gyi + 1 >= originalHeight ? gyi : gyi + 1);
            sf::Color c11 = sourceImage.getPixel(gxi + 1 >= originalWidth ? gxi : gxi + 1,
                                                 gyi + 1 >= originalHeight ? gyi : gyi + 1);

            float dx = gx - gxi;
            float dy = gy - gyi;
            sf::Color result(
                static_cast<sf::Uint8>((c00.r * (1 - dx) + c10.r * dx) * (1 - dy) + (c01.r * (1 - dx) + c11.r * dx) * dy),
                static_cast<sf::Uint8>((c00.g * (1 - dx) + c10.g * dx) * (1 - dy) + (c01.g * (1 - dx) + c11.g * dx) * dy),
                static_cast<sf::Uint8>((c00.b * (1 - dx) + c10.b * dx) * (1 - dy) + (c01.b * (1 - dx) + c11.b * dx) * dy),
                static_cast<sf::Uint8>((c00.a * (1 - dx) + c10.a * dx) * (1 - dy) + (c01.a * (1 - dx) + c11.a * dx) * dy)
            );

            scaledImage.setPixel(x, y, result);
        }
    }

    return scaledImage;
}

////////////////////////////////////////////////////////////
Glyph ColorFont::loadGlyph(Uint32 codePoint, unsigned int characterSize, bool bold, float outlineThickness) const
{
    // The glyph to return
    Glyph glyph;

    // First, transform our ugly void* to a FT_Face
    FT_Face face = static_cast<FT_Face>(m_face);
    if (!face)
        return glyph;

    // Set the character size
    int renderedSize = setCurrentSize(characterSize);
    if (renderedSize == 0){
        err() << "Can't set size for char: " << codePoint << '\n';
        return glyph;
    }

    // Load the glyph corresponding to the code point
    FT_Int32 flags = (FT_HAS_COLOR(face) ? FT_LOAD_COLOR : FT_LOAD_TARGET_NORMAL) | FT_LOAD_FORCE_AUTOHINT;

    if (outlineThickness != 0)
        flags |= FT_LOAD_NO_BITMAP;
    if (FT_Load_Char(face, codePoint, flags) != 0){
        err() << "Can't load char: " << codePoint << std::endl;
        return glyph;
    }

    // Retrieve the glyph
    FT_Glyph glyphDesc;
    if (FT_Get_Glyph(face->glyph, &glyphDesc) != 0){
        err() << "Can't get glyph for char: " << codePoint << std::endl;
        return glyph;
    }

    // Apply bold and outline (there is no fallback for outline) if necessary -- first technique using outline (highest quality)
    FT_Pos weight = 1 << 6;
    bool outline = (glyphDesc->format == FT_GLYPH_FORMAT_OUTLINE);
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

    // Convert the glyph to a bitmap (i.e. rasterize it)
    // Warning! After this line, do not read any data from glyphDesc directly, use
    // bitmapGlyph.root to access the FT_Glyph data.
    auto ft_err = FT_Glyph_To_Bitmap(&glyphDesc, FT_RENDER_MODE_NORMAL, 0, 1);

    if(ft_err){
        err() << "Can't convert glyph to bitmap for char: " << codePoint << std::endl;
        err() << "Code: " << FT_Error_String(ft_err) << std::endl;
        return glyph;
    }

    FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyphDesc);
    FT_Bitmap& bitmap = bitmapGlyph->bitmap;

    // Apply bold if necessary -- fallback technique using bitmap (lower quality)
    if (!outline)
    {
        if (bold)
            FT_Bitmap_Embolden(static_cast<FT_Library>(m_library), &bitmap, weight, weight);

        if (outlineThickness != 0)
            err() << "Failed to outline glyph (no fallback available)" << std::endl;
    }

    auto scaleFactor = characterSize / float(renderedSize);

    // Compute the glyph's advance offset
    glyph.advance = static_cast<float>(bitmapGlyph->root.advance.x >> 16);
    if (bold)
        glyph.advance += static_cast<float>(weight) / static_cast<float>(1 << 6);

    glyph.advance *= scaleFactor;

    glyph.lsbDelta = static_cast<int>(face->glyph->lsb_delta) * scaleFactor;
    glyph.rsbDelta = static_cast<int>(face->glyph->rsb_delta) * scaleFactor;


    unsigned int width  = bitmap.width * scaleFactor;
    unsigned int height = bitmap.rows * scaleFactor;

    if ((width > 0) && (height > 0))
    {
        // Leave a small padding around characters, so that filtering doesn't
        // pollute them with pixels from neighbors
        const unsigned int padding = 2;

        width += 2 * padding;
        height += 2 * padding;

        // Get the glyphs page corresponding to the character size
        Page& page = loadPage(characterSize);

        // Find a good position for the new glyph into the texture
        glyph.textureRect = findGlyphRect(page, width, height);

        // Make sure the texture data is positioned in the center
        // of the allocated texture rectangle
        glyph.textureRect.left   += static_cast<int>(padding);
        glyph.textureRect.top    += static_cast<int>(padding);
        glyph.textureRect.width  -= static_cast<int>(2 * padding);
        glyph.textureRect.height -= static_cast<int>(2 * padding);

        // Compute the glyph's bounding box
        glyph.bounds.left   = static_cast<float>( bitmapGlyph->left * scaleFactor);
        glyph.bounds.top    = static_cast<float>(-bitmapGlyph->top * scaleFactor);
        glyph.bounds.width  = static_cast<float>( bitmap.width * scaleFactor);
        glyph.bounds.height = static_cast<float>( bitmap.rows * scaleFactor);

        // Resize the pixel buffer to the new size and fill it with transparent white pixels
        m_pixelBuffer.resize(width * height * 4);

        Uint8* current = &m_pixelBuffer[0];
        Uint8* end = current + width * height * 4;

        while (current != end)
        {
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 0;
        }

        // Extract the glyph's pixels from the bitmap
        const Uint8* pixels = bitmap.buffer;
        if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
        {
            // Pixels are 1 bit monochrome values
            for (unsigned int y = padding; y < height - padding; ++y)
            {
                for (unsigned int x = padding; x < width - padding; ++x)
                {
                    // The color channels remain white, just fill the alpha channel
                    std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = ((pixels[(x - padding) / 8]) & (1 << (7 - ((x - padding) % 8)))) ? 255 : 0;
                }
                pixels += bitmap.pitch;
            }
        }
        else if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) 
        {
            sf::Image emoji;
            emoji.create(bitmap.width, bitmap.rows);

            for (unsigned int y = 0; y < bitmap.rows; ++y)
            {
                for (unsigned int x = 0; x < bitmap.width; ++x)
                {
                    // The color channels remain white, just fill the alpha channel
                    std::size_t sourceIndex = (x) * 4;
                    std::size_t index = x + y * bitmap.width;

                    emoji.setPixel(x, y, {
                        pixels[sourceIndex + 2],
                        pixels[sourceIndex + 1],
                        pixels[sourceIndex + 0],
                        pixels[sourceIndex + 3]
                    });
                }

                pixels += bitmap.pitch;
            }

            if(renderedSize != characterSize)
                emoji = ScaleImage(emoji, scaleFactor);

            for (unsigned int y = 0; y < emoji.getSize().y; ++y)
            {
                for (unsigned int x = 0; x < emoji.getSize().x; ++x)
                {
                    auto pixel = emoji.getPixel(x, y);

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
            // Pixels are 8 bits gray levels
            for (unsigned int y = padding; y < height - padding; ++y)
            {
                for (unsigned int x = padding; x < width - padding; ++x)
                {
                    // The color channels remain white, just fill the alpha channel
                    std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = pixels[x - padding];
                }
                pixels += bitmap.pitch;
            }
        }

        // Write the pixels to the texture
        unsigned int x = static_cast<unsigned int>(glyph.textureRect.left) - padding;
        unsigned int y = static_cast<unsigned int>(glyph.textureRect.top) - padding;
        unsigned int w = static_cast<unsigned int>(glyph.textureRect.width) + 2 * padding;
        unsigned int h = static_cast<unsigned int>(glyph.textureRect.height) + 2 * padding;
        page.texture.update(&m_pixelBuffer[0], w, h, x, y);
    }

    // Delete the FT glyph
    FT_Done_Glyph(glyphDesc);

    // Done :)
    return glyph;
}


////////////////////////////////////////////////////////////
IntRect ColorFont::findGlyphRect(Page& page, unsigned int width, unsigned int height) const
{
    // Find the line that fits well the glyph
    Row* row = NULL;
    float bestRatio = 0;
    for (std::vector<Row>::iterator it = page.rows.begin(); it != page.rows.end() && !row; ++it)
    {
        float ratio = static_cast<float>(height) / static_cast<float>(it->height);

        // Ignore rows that are either too small or too high
        if ((ratio < 0.7f) || (ratio > 1.f))
            continue;

        // Check if there's enough horizontal space left in the row
        if (width > page.texture.getSize().x - it->width)
            continue;

        // Make sure that this new row is the best found so far
        if (ratio < bestRatio)
            continue;

        // The current row passed all the tests: we can select it
        row = &*it;
        bestRatio = ratio;
    }

    // If we didn't find a matching row, create a new one (10% taller than the glyph)
    if (!row)
    {
        unsigned int rowHeight = height + height / 10;
        while ((page.nextRow + rowHeight >= page.texture.getSize().y) || (width >= page.texture.getSize().x))
        {
            // Not enough space: resize the texture if possible
            unsigned int textureWidth  = page.texture.getSize().x;
            unsigned int textureHeight = page.texture.getSize().y;
            if ((textureWidth * 2 <= Texture::getMaximumSize()) && (textureHeight * 2 <= Texture::getMaximumSize()))
            {
                // Make the texture 2 times bigger
                Texture newTexture;
                newTexture.create(textureWidth * 2, textureHeight * 2);
                newTexture.setSmooth(m_isSmooth);
                newTexture.update(page.texture);
                page.texture.swap(newTexture);
            }
            else
            {
                // Oops, we've reached the maximum texture size...
                err() << "Failed to add a new character to the font: the maximum texture size has been reached" << std::endl;
                return IntRect(0, 0, 2, 2);
            }
        }

        // We can now create the new row
        page.rows.push_back(Row(page.nextRow, rowHeight));
        page.nextRow += rowHeight;
        row = &page.rows.back();
    }

    // Find the glyph's rectangle on the selected row
    IntRect rect(Rect<unsigned int>(row->width, row->top, width, height));

    // Update the row informations
    row->width += width;

    return rect;
}


////////////////////////////////////////////////////////////
int ColorFont::setCurrentSize(unsigned int characterSize) const
{
    // FT_Set_Pixel_Sizes is an expensive function, so we must call it
    // only when necessary to avoid killing performances

    FT_Face face = static_cast<FT_Face>(m_face);
    FT_UShort currentSize = face->size->metrics.x_ppem;


    if (currentSize != characterSize)
    {
        if (FT_HAS_COLOR(face) && face->available_sizes) {
            int best_match = 0;
            int diff = std::labs(characterSize - face->available_sizes[0].width);
            for (int i = 1; i < face->num_fixed_sizes; ++i) {
                int ndiff =
                std::labs(characterSize - face->available_sizes[i].width);
                if (ndiff < diff) {
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
            // In the case of bitmap fonts, resizing can
            // fail if the requested size is not available
            if (!FT_IS_SCALABLE(face))
            {
                err() << "Failed to set bitmap font size to " << characterSize << std::endl;
                err() << "Available sizes are: ";
                for (int i = 0; i < face->num_fixed_sizes; ++i)
                {
                    const long size = (face->available_sizes[i].y_ppem + 32) >> 6;
                    err() << size << " ";
                }
                err() << std::endl;
            }
            else
            {
                err() << "Failed to set font size to " << characterSize << std::endl;
            }
        }
        return result == FT_Err_Ok ? characterSize : 0;
    }

    return characterSize;
}

ColorFont::Page::Page(bool smooth) :
    nextRow(3)
{
    // Make sure that the texture is initialized by default
    sf::Image image;
    image.create(128, 128, Color(255, 255, 255, 0));

    // Reserve a 2x2 white square for texturing underlines
    for (unsigned int x = 0; x < 2; ++x)
        for (unsigned int y = 0; y < 2; ++y)
            image.setPixel(x, y, Color(255, 255, 255, 255));

    // Create the texture
    texture.loadFromImage(image);
    texture.setSmooth(smooth);
}
