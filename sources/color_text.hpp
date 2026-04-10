#pragma once

#include "color_font.hpp"
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/System/String.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/Text.hpp>
#include <cstdint>

class ColorText : public sf::Drawable, public sf::Transformable
{
public:

    ColorText();

    ColorText(const sf::String& string, const ColorFont& font, unsigned int characterSize = 30);

    void setString(const sf::String& string);

    void setFont(const ColorFont& font);

    void setCharacterSize(unsigned int size);

    void setLineSpacing(float spacingFactor);

    void setLetterSpacing(float spacingFactor);

    void setStyle(sf::Text::Style style);

    void setFillColor(const sf::Color& color);

    void setOutlineColor(const sf::Color& color);

    void setOutlineThickness(float thickness);

    const sf::String& getString() const;

    const ColorFont* getFont() const;

    unsigned int getCharacterSize() const;

    float getLetterSpacing() const;

    float getLineSpacing() const;

    sf::Text::Style getStyle() const;

    const sf::Color& getFillColor() const;

    const sf::Color& getOutlineColor() const;

    float getOutlineThickness() const;

    sf::Vector2f findCharacterPos(std::size_t index) const;

    sf::FloatRect getLocalBounds() const;

    sf::FloatRect getGlobalBounds() const;

private:

    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;

    void ensureGeometryUpdate() const;

    sf::String              m_string;
    const ColorFont*        m_font;
    unsigned int            m_characterSize;
    float                   m_letterSpacingFactor;
    float                   m_lineSpacingFactor;
    sf::Text::Style         m_style;
    sf::Color               m_fillColor;
    sf::Color               m_outlineColor;
    float                   m_outlineThickness;
    mutable sf::VertexArray m_vertices;
    mutable sf::VertexArray m_outlineVertices;
    mutable sf::FloatRect   m_bounds;
    mutable bool            m_geometryNeedUpdate;
    mutable uint64_t        m_fontTextureId;
};
