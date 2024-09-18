#pragma once

#include "color_font.hpp"
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/System/String.hpp>
#include <SFML/Graphics/VertexArray.hpp>

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

    void setStyle(sf::Uint32 style);

    void setFillColor(const sf::Color& color);

    void setOutlineColor(const sf::Color& color);

    void setOutlineThickness(float thickness);

    const sf::String& getString() const;

    const ColorFont* getFont() const;

    unsigned int getCharacterSize() const;

    float getLetterSpacing() const;

    float getLineSpacing() const;

    sf::Uint32 getStyle() const;

    const sf::Color& getFillColor() const;

    const sf::Color& getOutlineColor() const;

    float getOutlineThickness() const;

    sf::Vector2f findCharacterPos(std::size_t index) const;

    sf::FloatRect getLocalBounds() const;

    sf::FloatRect getGlobalBounds() const;

private:

    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;

    void ensureGeometryUpdate() const;

    sf::String              m_string;              //!< String to display
    const ColorFont*         m_font;                //!< Font used to display the string
    unsigned int        m_characterSize;       //!< Base size of characters, in pixels
    float               m_letterSpacingFactor; //!< Spacing factor between letters
    float               m_lineSpacingFactor;   //!< Spacing factor between lines
    sf::Uint32              m_style;               //!< Text style (see Style enum)
    sf::Color               m_fillColor;           //!< Text fill color
    sf::Color               m_outlineColor;        //!< Text outline color
    float               m_outlineThickness;    //!< Thickness of the text's outline
    mutable sf::VertexArray m_vertices;            //!< Vertex array containing the fill geometry
    mutable sf::VertexArray m_outlineVertices;     //!< Vertex array containing the outline geometry
    mutable sf::FloatRect   m_bounds;              //!< Bounding rectangle of the text (in local coordinates)
    mutable bool        m_geometryNeedUpdate;  //!< Does the geometry need to be recomputed?
    mutable sf::Uint64      m_fontTextureId;       //!< The font texture id
};
