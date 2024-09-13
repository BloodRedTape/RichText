#pragma once

#include <optional>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>

class RichFont {
	std::vector<sf::Font> m_Fonts;
public:
	RichFont(std::vector<sf::Font> &&fonts);

	bool valid()const;

	const sf::Font *findFontForGlyph(std::uint32_t codepoint)const;

	static RichFont loadFromFile(const std::string &filepath);

	static RichFont loadFromFiles(std::initializer_list<std::string> filepath);
};

class RichTextLine: public sf::Drawable, public sf::Transformable{
private:
	std::vector<sf::Text> m_Texts;
	sf::String m_String;
	const RichFont *m_Font = nullptr;
	int m_CharacterSize = 0;
public:
    sf::FloatRect getLocalBounds()const;

	void setString(const sf::String &string);

	void setString(const std::string &string);

	sf::String getString()const;

	void setCharacterSize(int size);

	void setRichFont(const RichFont &font);

	void setFillColor(const sf::Color &color);

	void setOutlineColor(const sf::Color &color);

	void setOutlineThickness(float thickness);

	void setStyle(sf::Text::Style style);

	bool drawn()const;
protected:
	static std::vector<sf::Text> build(const RichFont &font, const sf::String &string, int character_size);

	void rebuild(const sf::String &string);

	virtual void rebuild();

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
};

class ElipsisRichTextLine : public RichTextLine {
	int m_MaxWidth = 0;
public:
	void setMaxWidth(int width);

	void rebuild()override;
};

