#include "color_text.hpp"

#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <cmath>

using namespace sf;

namespace
{
    void addLine(sf::VertexArray& vertices, float lineLength, float lineTop, const sf::Color& color, float offset, float thickness, float outlineThickness = 0)
    {
        float top    = std::floor(lineTop + offset - (thickness / 2) + 0.5f);
        float bottom = top + std::floor(thickness + 0.5f);

        vertices.append(sf::Vertex{sf::Vector2f(-outlineThickness,             top    - outlineThickness), color, sf::Vector2f(1, 1)});
        vertices.append(sf::Vertex{sf::Vector2f(lineLength + outlineThickness, top    - outlineThickness), color, sf::Vector2f(1, 1)});
        vertices.append(sf::Vertex{sf::Vector2f(-outlineThickness,             bottom + outlineThickness), color, sf::Vector2f(1, 1)});
        vertices.append(sf::Vertex{sf::Vector2f(-outlineThickness,             bottom + outlineThickness), color, sf::Vector2f(1, 1)});
        vertices.append(sf::Vertex{sf::Vector2f(lineLength + outlineThickness, top    - outlineThickness), color, sf::Vector2f(1, 1)});
        vertices.append(sf::Vertex{sf::Vector2f(lineLength + outlineThickness, bottom + outlineThickness), color, sf::Vector2f(1, 1)});
    }

    void addGlyphQuad(sf::VertexArray& vertices, sf::Vector2f position, const sf::Color& color, const sf::Glyph& glyph, float italicShear)
    {
        float padding = 1.0;

        float left   = glyph.bounds.position.x - padding;
        float top    = glyph.bounds.position.y - padding;
        float right  = glyph.bounds.position.x + glyph.bounds.size.x + padding;
        float bottom = glyph.bounds.position.y + glyph.bounds.size.y + padding;

        float u1 = static_cast<float>(glyph.textureRect.position.x) - padding;
        float v1 = static_cast<float>(glyph.textureRect.position.y) - padding;
        float u2 = static_cast<float>(glyph.textureRect.position.x + glyph.textureRect.size.x) + padding;
        float v2 = static_cast<float>(glyph.textureRect.position.y + glyph.textureRect.size.y) + padding;

        vertices.append(sf::Vertex{sf::Vector2f(position.x + left  - italicShear * top,    position.y + top),    color, sf::Vector2f(u1, v1)});
        vertices.append(sf::Vertex{sf::Vector2f(position.x + right - italicShear * top,    position.y + top),    color, sf::Vector2f(u2, v1)});
        vertices.append(sf::Vertex{sf::Vector2f(position.x + left  - italicShear * bottom, position.y + bottom), color, sf::Vector2f(u1, v2)});
        vertices.append(sf::Vertex{sf::Vector2f(position.x + left  - italicShear * bottom, position.y + bottom), color, sf::Vector2f(u1, v2)});
        vertices.append(sf::Vertex{sf::Vector2f(position.x + right - italicShear * top,    position.y + top),    color, sf::Vector2f(u2, v1)});
        vertices.append(sf::Vertex{sf::Vector2f(position.x + right - italicShear * bottom, position.y + bottom), color, sf::Vector2f(u2, v2)});
    }
}


ColorText::ColorText() :
m_string             (),
m_font               (nullptr),
m_characterSize      (30),
m_letterSpacingFactor(1.f),
m_lineSpacingFactor  (1.f),
m_style              (sf::Text::Style::Regular),
m_fillColor          (255, 255, 255),
m_outlineColor       (0, 0, 0),
m_outlineThickness   (0),
m_vertices           (sf::PrimitiveType::Triangles),
m_outlineVertices    (sf::PrimitiveType::Triangles),
m_bounds             (),
m_geometryNeedUpdate (false),
m_fontTextureId      (0)
{
}


////////////////////////////////////////////////////////////
ColorText::ColorText(const sf::String& string, const ColorFont& font, unsigned int characterSize) :
m_string             (string),
m_font               (&font),
m_characterSize      (characterSize),
m_letterSpacingFactor(1.f),
m_lineSpacingFactor  (1.f),
m_style              (sf::Text::Style::Regular),
m_fillColor          (255, 255, 255),
m_outlineColor       (0, 0, 0),
m_outlineThickness   (0),
m_vertices           (sf::PrimitiveType::Triangles),
m_outlineVertices    (sf::PrimitiveType::Triangles),
m_bounds             (),
m_geometryNeedUpdate (true),
m_fontTextureId      (0)
{
}


////////////////////////////////////////////////////////////
void ColorText::setString(const sf::String& string)
{
    if (m_string != string)
    {
        m_string = string;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void ColorText::setFont(const ColorFont& font)
{
    if (m_font != &font)
    {
        m_font = &font;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void ColorText::setCharacterSize(unsigned int size)
{
    if (m_characterSize != size)
    {
        m_characterSize = size;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void ColorText::setLetterSpacing(float spacingFactor)
{
    if (m_letterSpacingFactor != spacingFactor)
    {
        m_letterSpacingFactor = spacingFactor;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void ColorText::setLineSpacing(float spacingFactor)
{
    if (m_lineSpacingFactor != spacingFactor)
    {
        m_lineSpacingFactor = spacingFactor;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void ColorText::setStyle(sf::Text::Style style)
{
    if (m_style != style)
    {
        m_style = style;
        m_geometryNeedUpdate = true;
    }
}

void ColorText::setFillColor(const sf::Color& color)
{
    if (color != m_fillColor)
    {
        m_fillColor = color;

        if (!m_geometryNeedUpdate)
        {
            auto real_fill_color = m_font && m_font->isColorEmojiFont() ? sf::Color::White : m_fillColor;
            for (std::size_t i = 0; i < m_vertices.getVertexCount(); ++i)
                m_vertices[i].color = real_fill_color;
        }
    }
}


////////////////////////////////////////////////////////////
void ColorText::setOutlineColor(const sf::Color& color)
{
    if (color != m_outlineColor)
    {
        m_outlineColor = color;

        if (!m_geometryNeedUpdate)
        {
            for (std::size_t i = 0; i < m_outlineVertices.getVertexCount(); ++i)
                m_outlineVertices[i].color = m_outlineColor;
        }
    }
}


////////////////////////////////////////////////////////////
void ColorText::setOutlineThickness(float thickness)
{
    if (thickness != m_outlineThickness)
    {
        m_outlineThickness = thickness;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
const sf::String& ColorText::getString() const
{
    return m_string;
}


////////////////////////////////////////////////////////////
const ColorFont* ColorText::getFont() const
{
    return m_font;
}


////////////////////////////////////////////////////////////
unsigned int ColorText::getCharacterSize() const
{
    return m_characterSize;
}


////////////////////////////////////////////////////////////
float ColorText::getLetterSpacing() const
{
    return m_letterSpacingFactor;
}


////////////////////////////////////////////////////////////
float ColorText::getLineSpacing() const
{
    return m_lineSpacingFactor;
}


////////////////////////////////////////////////////////////
sf::Text::Style ColorText::getStyle() const
{
    return m_style;
}


////////////////////////////////////////////////////////////
const sf::Color& ColorText::getFillColor() const
{
    return m_fillColor;
}


////////////////////////////////////////////////////////////
const sf::Color& ColorText::getOutlineColor() const
{
    return m_outlineColor;
}


////////////////////////////////////////////////////////////
float ColorText::getOutlineThickness() const
{
    return m_outlineThickness;
}


////////////////////////////////////////////////////////////
sf::Vector2f ColorText::findCharacterPos(std::size_t index) const
{
    if (!m_font)
        return Vector2f();

    if (index > m_string.getSize())
        index = m_string.getSize();

    bool  isBold          = m_style & sf::Text::Style::Bold;
    float whitespaceWidth = m_font->getGlyph(L' ', m_characterSize, isBold).advance;
    float letterSpacing   = (whitespaceWidth / 3.f) * (m_letterSpacingFactor - 1.f);
    whitespaceWidth      += letterSpacing;
    float lineSpacing     = m_font->getLineSpacing(m_characterSize) * m_lineSpacingFactor;

    Vector2f  position;
    uint32_t  prevChar = 0;
    for (std::size_t i = 0; i < index; ++i)
    {
        uint32_t curChar = m_string[i];

        position.x += m_font->getKerning(prevChar, curChar, m_characterSize, isBold);
        prevChar = curChar;

        switch (curChar)
        {
            case ' ':  position.x += whitespaceWidth;             continue;
            case '\t': position.x += whitespaceWidth * 4;         continue;
            case '\n': position.y += lineSpacing; position.x = 0; continue;
        }

        position.x += m_font->getGlyph(curChar, m_characterSize, isBold).advance + letterSpacing;
    }

    position = getTransform().transformPoint(position);

    return position;
}


////////////////////////////////////////////////////////////
sf::FloatRect ColorText::getLocalBounds() const
{
    ensureGeometryUpdate();

    return m_bounds;
}


////////////////////////////////////////////////////////////
sf::FloatRect ColorText::getGlobalBounds() const
{
    return getTransform().transformRect(getLocalBounds());
}


////////////////////////////////////////////////////////////
void ColorText::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if (m_font)
    {
        ensureGeometryUpdate();

        states.transform *= getTransform();
        states.texture = &m_font->getTexture(m_characterSize);

        if (m_outlineThickness != 0)
            target.draw(m_outlineVertices, states);

        target.draw(m_vertices, states);
    }
}


////////////////////////////////////////////////////////////
void ColorText::ensureGeometryUpdate() const
{
    if (!m_font)
        return;

    if (!m_geometryNeedUpdate)
        return;

    m_fontTextureId = 777;

    m_geometryNeedUpdate = false;

    m_vertices.clear();
    m_outlineVertices.clear();
    m_bounds = FloatRect();

    if (m_string.isEmpty())
        return;

    bool  isBold             = m_style & sf::Text::Style::Bold;
    bool  isUnderlined       = m_style & sf::Text::Style::Underlined;
    bool  isStrikeThrough    = m_style & sf::Text::Style::StrikeThrough;
    float italicShear        = (m_style & sf::Text::Style::Italic) ? 0.209f : 0.f;
    float underlineOffset    = m_font->getUnderlinePosition(m_characterSize);
    float underlineThickness = m_font->getUnderlineThickness(m_characterSize);

    FloatRect xBounds = m_font->getGlyph(L'x', m_characterSize, isBold).bounds;
    float strikeThroughOffset = xBounds.position.y + xBounds.size.y / 2.f;

    float whitespaceWidth = m_font->getGlyph(L' ', m_characterSize, isBold).advance;
    float letterSpacing   = (whitespaceWidth / 3.f) * (m_letterSpacingFactor - 1.f);
    whitespaceWidth      += letterSpacing;
    float lineSpacing     = m_font->getLineSpacing(m_characterSize) * m_lineSpacingFactor;
    float x               = 0.f;
    float y               = static_cast<float>(m_characterSize);

    float minX = static_cast<float>(m_characterSize);
    float minY = static_cast<float>(m_characterSize);
    float maxX = 0.f;
    float maxY = 0.f;
    uint32_t prevChar = 0;
    for (std::size_t i = 0; i < m_string.getSize(); ++i)
    {
        uint32_t curChar = m_string[i];

        if (curChar == L'\r')
            continue;

        x += m_font->getKerning(prevChar, curChar, m_characterSize, isBold);

        if (isUnderlined && (curChar == L'\n' && prevChar != L'\n'))
        {
            addLine(m_vertices, x, y, m_fillColor, underlineOffset, underlineThickness);

            if (m_outlineThickness != 0)
                addLine(m_outlineVertices, x, y, m_outlineColor, underlineOffset, underlineThickness, m_outlineThickness);
        }

        if (isStrikeThrough && (curChar == L'\n' && prevChar != L'\n'))
        {
            addLine(m_vertices, x, y, m_fillColor, strikeThroughOffset, underlineThickness);

            if (m_outlineThickness != 0)
                addLine(m_outlineVertices, x, y, m_outlineColor, strikeThroughOffset, underlineThickness, m_outlineThickness);
        }

        prevChar = curChar;

        if ((curChar == L' ') || (curChar == L'\n') || (curChar == L'\t'))
        {
            minX = std::min(minX, x);
            minY = std::min(minY, y);

            switch (curChar)
            {
                case L' ':  x += whitespaceWidth;     break;
                case L'\t': x += whitespaceWidth * 4; break;
                case L'\n': y += lineSpacing; x = 0;  break;
            }

            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);

            continue;
        }

        if (m_outlineThickness != 0)
        {
            const Glyph& glyph = m_font->getGlyph(curChar, m_characterSize, isBold, m_outlineThickness);
            addGlyphQuad(m_outlineVertices, Vector2f(x, y), m_outlineColor, glyph, italicShear);
        }

        const Glyph& glyph = m_font->getGlyph(curChar, m_characterSize, isBold);

        auto real_fill_color = m_font->isColorEmojiFont() ? sf::Color::White : m_fillColor;
        addGlyphQuad(m_vertices, Vector2f(x, y), real_fill_color, glyph, italicShear);

        float left   = glyph.bounds.position.x;
        float top    = glyph.bounds.position.y;
        float right  = glyph.bounds.position.x + glyph.bounds.size.x;
        float bottom = glyph.bounds.position.y  + glyph.bounds.size.y;

        minX = std::min(minX, x + left  - italicShear * bottom);
        maxX = std::max(maxX, x + right - italicShear * top);
        minY = std::min(minY, y + top);
        maxY = std::max(maxY, y + bottom);

        x += glyph.advance + letterSpacing;
    }

    if (m_outlineThickness != 0)
    {
        float outline = std::abs(std::ceil(m_outlineThickness));
        minX -= outline;
        maxX += outline;
        minY -= outline;
        maxY += outline;
    }

    if (isUnderlined && (x > 0))
    {
        addLine(m_vertices, x, y, m_fillColor, underlineOffset, underlineThickness);

        if (m_outlineThickness != 0)
            addLine(m_outlineVertices, x, y, m_outlineColor, underlineOffset, underlineThickness, m_outlineThickness);
    }

    if (isStrikeThrough && (x > 0))
    {
        addLine(m_vertices, x, y, m_fillColor, strikeThroughOffset, underlineThickness);

        if (m_outlineThickness != 0)
            addLine(m_outlineVertices, x, y, m_outlineColor, strikeThroughOffset, underlineThickness, m_outlineThickness);
    }

    m_bounds.position.x = minX;
    m_bounds.position.y = minY;
    m_bounds.size.x     = maxX - minX;
    m_bounds.size.y     = maxY - minY;
}
