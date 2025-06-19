#include "rich_text.hpp"
#include <bsl/log.hpp>
#include <queue>
#include <SFML/Graphics/RenderTarget.hpp>

DEFINE_LOG_CATEGORY(RichText)

RichFont::RichFont(std::vector<ColorFont>&& fonts):
	m_Fonts(std::move(fonts))
{}

bool RichFont::valid() const{
	return m_Fonts.size();
}

const ColorFont *RichFont::findFontForGlyph(std::uint32_t codepoint) const{
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

float RichFont::getAscent(unsigned int characterSize) const {
    if(!m_Fonts.size())
        return 0.f;

    return m_Fonts.front().getAscent(characterSize);
}

float RichFont::getDescent(unsigned int characterSize) const {
    if(!m_Fonts.size())
        return 0.f;

    return m_Fonts.front().getDescent(characterSize);
}

RichFont RichFont::loadFromFile(const std::string& filepath){
	return loadFromFiles({filepath});
}

RichFont RichFont::loadFromFiles(std::initializer_list<std::string> filepath){
	std::vector<ColorFont> fonts;
	
	for (const auto& path: filepath) {
		ColorFont font;
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
sf::Vector2f RichTextLine::getTypographicSize()const {
    return {getLocalBounds().getSize().x, float(m_CharacterSize)};
}

float RichTextLine::getMaxLineHeight()const {
    return getAscent() + getDescent();
}

float RichTextLine::getAscent()const {
    if(!m_Font)
        return 0.f;

    return m_Font->getAscent(m_CharacterSize);
}

float RichTextLine::getDescent()const {
    if(!m_Font)
        return 0.f;

    return m_Font->getDescent(m_CharacterSize);
}

void RichTextLine::setString(const sf::String& string){
    m_String = string;
    
    rebuild();
}

void RichTextLine::setString(const std::string& string){
    setString(sf::String::fromUtf8(string.begin(), string.end()));
}

sf::String RichTextLine::getString() const{
    return m_String;
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

bool RichTextLine::drawn() const{
    return m_CharacterSize && m_Font && m_String.getSize();
}

std::vector<ColorText> RichTextLine::build(const RichFont &rich_font, const sf::String& string, int character_size){
    if (!rich_font.valid()) {
        LogRichText(Error, "Using invalid font for text line");
        return {};
    }
    
    std::vector<ColorText> texts;
    sf::String last_string;
    const ColorFont* last_font = nullptr;
    
    sf::Vector2f position;

    auto Flush = [&]() {
        if (!last_string.isEmpty()) {
            ColorText text(last_string, *last_font, character_size);
            text.setPosition(position);

            //text.setOutlineThickness(outline);
            position.x += text.getLocalBounds().width; //+ outline * 2; // Consider outline thickness

            texts.emplace_back(std::move(text));
        }

        last_string = {};
        last_font = nullptr;
    };
    
    for (const auto& character : string) {
        const ColorFont* font = rich_font.findFontForGlyph(character);

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

void RichTextLine::rebuild(const sf::String& string){
    if(!drawn()){
        m_Texts = {};
        return;
    }

    m_Texts = RichTextLine::build(*m_Font, string, m_CharacterSize);
}

void RichTextLine::rebuild(){
    rebuild(m_String);
}

void RichTextLine::draw(sf::RenderTarget& target, sf::RenderStates states) const{
    if(!m_Texts.size())
        return;

    states.transform *= getTransform();

    for (const auto &text : m_Texts) {
        target.draw(text, states);
    }
}

void ElipsisRichTextLine::setMaxWidth(int width){
    m_MaxWidth = width;

    rebuild();
}

void ElipsisRichTextLine::rebuild(){
    RichTextLine::rebuild();

    if(!m_MaxWidth || !drawn())
        return;

    sf::String initial = getString();

    while (getLocalBounds().width > m_MaxWidth) {
        if (!initial.getSize()) {
            RichTextLine::rebuild("");
            LogRichText(Error, "elipsis can't fit any text into % width", m_MaxWidth);
            return;
        }
        initial.erase(initial.getSize() - 1);
        RichTextLine::rebuild(initial + L"...");
    }
}


sf::FloatRect RichText::getLocalBounds() const{
    sf::FloatRect bounds;
    for (const auto& line: m_Lines) {
        sf::FloatRect localBounds = line.getLocalBounds();
        sf::Vector2f position = line.getPosition();
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

sf::Vector2f RichText::getTypographicSize()const {
    if(!m_Lines.size())
        return {};

    auto spacing = m_LineSpacing;
    
    return {getLocalBounds().getSize().x, float(spacing * (m_Lines.size() - 1) + m_CharacterSize)};
}

float RichText::getAscent()const {
    if(!m_Font)
        return 0.f;

    return m_Font->getAscent(m_CharacterSize);
}

float RichText::getDescent()const {
    if(!m_Font)
        return 0.f;

    return m_Font->getDescent(m_CharacterSize);
}

void RichText::setString(const sf::String& string){
    m_String = string;

    rebuild();
}

void RichText::setString(const std::string& string){
    setString(sf::String::fromUtf8(string.begin(), string.end()));
}

sf::String RichText::getString() const{
    return m_String;
}

void RichText::setCharacterSize(int size){
    m_CharacterSize = size;

    rebuild();
}

void RichText::setLineSpacing(int spacing)
{
    m_LineSpacing = spacing;

    rebuild();
}

void RichText::setRichFont(const RichFont& font){
    m_Font = &font;

    rebuild();
}

void RichText::setFillColor(const sf::Color& color){
    for(auto &line: m_Lines)
        line.setFillColor(color);
}

void RichText::setOutlineColor(const sf::Color& color){
    for(auto &line: m_Lines)
        line.setOutlineColor(color);
}

void RichText::setOutlineThickness(float thickness){
    for(auto &line: m_Lines)
        line.setOutlineThickness(thickness);
}

void RichText::setStyle(sf::Text::Style style){
    for(auto &line: m_Lines)
        line.setStyle(style);
}


void RichText::setAlignment(RichTextAlignment alignment){
    m_Alignment = alignment;

    rebuild();
}
void RichText::setWrapWidth(std::int32_t width) {
    m_WrapWidth = width;

    rebuild();
}

int RichText::getLinesCount() const{
    return m_Lines.size();
}

bool RichText::drawn() const{
    return m_CharacterSize && m_Font && m_String.getSize();
}

static float GetXForAlignment(const RichTextLine &line, RichTextAlignment alignment) {
    if(alignment == RichTextAlignment::Left)
        return 0;
    if(alignment == RichTextAlignment::Right)
        return -line.getLocalBounds().width;
    if(alignment == RichTextAlignment::Center)
        return -line.getLocalBounds().width / 2;
    return 0;
}

// Helper to check if a character is whitespace
static bool IsWhiteSpace(sf::Uint32 c) {
    return c == ' ';//std::iswspace(static_cast<wchar_t>(c));
}

static std::string RemoveLastWord(std::string &text) {
    if (!text.size()) return "";

    // Remove trailing whitespaces
    std::size_t end = text.size();
    while (end > 0 && IsWhiteSpace(text[end - 1])) {
        --end;
    }

    if (end == 0) return ""; // Only whitespaces in text

    // Find beginning of last word
    std::size_t start = end;
    while (start > 0 && !IsWhiteSpace(text[start - 1])) {
        --start;
    }

    // Extract last word
    std::string lastWord = text.substr(start, end - start);

    // Remove last word from text
    text.erase(start, text.size() - start);


    return lastWord;
}

static std::queue<std::string> Split(const std::string &string, char by) {
    std::queue<std::string> words;
    
    std::string current;
    for (auto ch : string) {

        if(ch == by){
            words.push(current);
            current = {};
            continue;
        }

        current += ch;
    }

    if(current.size())
        words.push(current);

    return words;
}

std::vector<RichTextLine> RichText::build(const RichFont& font, const sf::String& string, int character_size, int line_spacing, std::int32_t max_width, RichTextAlignment alignment){

    std::vector<RichTextLine> result;

    auto push_line = [&, offset = 0](const std::string &string) mutable {

        if(!string.size()){
            RichTextLine line;
            line.setString(std::string(""));
            line.setCharacterSize(character_size);
            line.setRichFont(font);
            result.push_back(line);

            offset += line_spacing;
        }

        std::queue<std::string> words = Split(string, ' ');

        RichTextLine line;
        line.setString(string);
        line.setCharacterSize(character_size);
        line.setRichFont(font);

        std::string line_text;

        auto push = [&]() {
            if(line_text.size()){
                if(line_text.size() > 1 && line_text.back() == ' ')
                    line_text.pop_back();
                line.setString(line_text);
            }else {
                line.setString(words.front());
                words.pop();
            }
            line.setPosition(sf::Vector2f(GetXForAlignment(line, alignment), offset));
            result.push_back(line);

            offset += line_spacing;

            line_text = {};
        };

        while (words.size()) {
            line.setString(line_text + words.front());

            if (line.getTypographicSize().x > max_width) {
                push();
                continue;
            }
            
            if(words.front().size())
                line_text += words.front() + ' ';
            words.pop();
        }

        if(line_text.size())
            push();
    };

    std::optional<std::string> current;

    for (auto ch : string.toUtf8()) {
        if (ch != '\n') {
            if(!current.has_value())
                current = "";
            current.value() += ch;
            continue;
        }
        
        push_line(current.value_or(""));
        current = {};
    }
    
    if(current.has_value())
        push_line(current.value());
    
    return result;
}

void RichText::rebuild(const sf::String& string){
    if(!drawn()){
        m_Lines = {};
        return;
    }

    m_Lines = RichText::build(*m_Font, string, m_CharacterSize, m_LineSpacing, m_WrapWidth, m_Alignment);
}

void RichText::rebuild(){
    rebuild(m_String);
}

void RichText::draw(sf::RenderTarget& target, sf::RenderStates states) const{
    if(!m_Lines.size())
        return;

    states.transform *= getTransform();

    for (const auto &line: m_Lines) {
        target.draw(line, states);
    }
}
