#include "rich_text.hpp"
#include <bsl/log.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

DEFINE_LOG_CATEGORY(RichText)

RichFont::RichFont(std::vector<sf::Font>&& fonts):
	m_Fonts(std::move(fonts))
{}

bool RichFont::valid() const{
	return m_Fonts.size();
}

const sf::Font *RichFont::findFontForGlyph(std::uint32_t codepoint) const{
	if(!m_Fonts.size()){
		LogRichText(Error, "Using invalid font");
		return nullptr;
	}

	for (const auto& font : m_Fonts) {
		if(font.hasGlyph(codepoint))
			return &font;
	}

	return &m_Fonts.front();
}

RichFont RichFont::loadFromFile(const std::string& filepath){
	return loadFromFiles({filepath});
}

RichFont RichFont::loadFromFiles(std::initializer_list<std::string> filepath){
	std::vector<sf::Font> fonts;
	
	for (const auto& path: filepath) {
		sf::Font font;
		if(!font.loadFromFile(path)){
			LogRichText(Error, "Can't load font from '%'", path);
			continue;
		}

		fonts.push_back(std::move(font));
	}

	return RichFont(std::move(fonts));
}

sf::FloatRect RichTextLine::getLocalBounds()const{
    sf::FloatRect bounds;
    for (const auto& text : m_Texts) {
        sf::FloatRect localBounds = text.getLocalBounds();
        sf::Vector2f position = text.getPosition();
        localBounds.left += position.x;
        localBounds.top += position.y;

        if (bounds.width == 0 && bounds.height == 0) {
            bounds = localBounds;
        } else {
            bounds.left = std::min(bounds.left, localBounds.left);
            bounds.top = std::min(bounds.top, localBounds.top);
            bounds.width = std::max(bounds.left + bounds.width, localBounds.left + localBounds.width) - bounds.left;
            bounds.height = std::max(bounds.top + bounds.height, localBounds.top + localBounds.height) - bounds.top;
        }
    }
    return bounds;
}

void RichTextLine::setString(const sf::String& string){
    m_String = string;
    
    rebuild();
}

void RichTextLine::setString(const std::string& string){
    setString(sf::String::fromUtf8(string.begin(), string.end()));
}

void RichTextLine::setCharacterSize(int size){
    m_CharacterSize = size;

    rebuild();
}

void RichTextLine::setRichFont(const RichFont& font){
    m_Font = &font;

    rebuild();
}

void RichTextLine::setFillColor(const sf::Color& color){
    for(auto &text: m_Texts)
        text.setFillColor(color);
}

void RichTextLine::setOutlineColor(const sf::Color& color){
    for(auto &text: m_Texts)
        text.setOutlineColor(color);
}

void RichTextLine::setOutlineThickness(float thickness){
    for(auto &text: m_Texts)
        text.setOutlineThickness(thickness);
}

void RichTextLine::setStyle(sf::Text::Style style){
    for(auto &text: m_Texts)
        text.setStyle(style);
}

std::vector<sf::Text> RichTextLine::build(const RichFont &rich_font, const sf::String& string, int character_size){
    if (!rich_font.valid()) {
        LogRichText(Error, "Using invalid font for text line");
        return {};
    }
    
    std::vector<sf::Text> texts;
    sf::String last_string;
    const sf::Font* last_font = nullptr;
    
    sf::Vector2f position;

    auto Flush = [&]() {
        if (!last_string.isEmpty()) {
            sf::Text text(last_string, *last_font, character_size);
            text.setPosition(position);

            //text.setOutlineThickness(outline);
            position.x += text.getLocalBounds().width; //+ outline * 2; // Consider outline thickness

            texts.emplace_back(std::move(text));
        }

        last_string = {};
        last_font = nullptr;
    };
    
    for (const auto& character : string) {
        const sf::Font* font = rich_font.findFontForGlyph(character);

        if (!font) 
            continue;

        if (font != last_font){
            Flush();
        }

        last_font = font;
        last_string += character;
    }
    
    Flush();

    return texts;
}

void RichTextLine::rebuild(){
    if(!m_CharacterSize || !m_Font || !m_String.getSize())
        return;

    m_Texts = RichTextLine::build(*m_Font, m_String, m_CharacterSize);
}

void RichTextLine::draw(sf::RenderTarget& target, sf::RenderStates states) const{
    states.transform *= getTransform();

    for (const auto &text : m_Texts) {
        target.draw(text, states);
    }
}
