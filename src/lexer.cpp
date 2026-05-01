#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace luma {
    static const std::unordered_map<std::string, TokenKind>
        KEYWORDS = {
        {"let",    TokenKind::Let},
        {"fn",     TokenKind::Fn},
        {"return", TokenKind::Return},
        {"if",     TokenKind::If},
        {"else",   TokenKind::Else},
        {"while",  TokenKind::While},
        {"for",    TokenKind::For},
        {"in",     TokenKind::In},
        {"true",   TokenKind::True},
        {"false",  TokenKind::False},
        {"null",   TokenKind::Null},
    };

    Lexer::Lexer(std::string_view src) : src_(src) {}

    char Lexer::advance() {
        char c = src_[pos_++];
        if (c == '\n') {
            ++line_;
            col_ = 1;
        }
        else {
            ++col_;
        }

        return c;
    }

    char Lexer::peek(int offset) const {
        size_t i = pos_ + offset;
        return i < src_.size() ? src_[i] : '\0';
    }

    bool Lexer::match(char expected) {
        if (pos_ < src_.size() && src_[pos_] == expected) {
            advance();
            return true;
        }

        return false;
    }

    Token Lexer::make(TokenKind k, std::string val) const {
        return Token{ k, std::move(val), line_, col_ };
    }

    void Lexer::skipWhitespaceAndComments() {
        while (pos_ < src_.size()) {
            char c = peek();
            if (std::isspace(c)) {
                advance();
            }
            else if (c == '/' && peek(1) == '/') {
                // line comment
                while (pos_ < src_.size() && peek() != '\n')
                    advance();
            }
            else if (c == '/' && peek(1) == '*') {
                // block comment
                while (pos_ < src_.size()) {
                    if (peek() == '*' && peek(1) == '/') {
                        advance();
                        advance();
                        break;
                    }
                }
            }
            else break;
        }
    }

    Token Lexer::readString() {
        std::string result;
        while (pos_ < src_.size() && peek() != '"') {
            char c = advance();
            if (c == '\\') {
                char esc = advance();
                switch (esc) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                default:   result += esc;  break;
                }
            }
            else {
                result += c;
            }
        }

        if (pos_ >= src_.size())
            return make(TokenKind::Error, "unterminated string");
        advance(); // closing ""
        return make(TokenKind::String, result);
    }

    Token Lexer::readNumber() {
        size_t start = pos_ - 1;
        bool has_dot = false;

        while (pos_ < src_.size() && (std::isdigit(peek()) || (peek() == '.' && !has_dot))) {
            if (peek() == '.') has_dot = true;
            advance();
        }

        return make(TokenKind::Number, std::string(src_.substr(start, pos_ - start)));
    }

    Token Lexer::readIdentOrKeyword() {
        size_t start = pos_ - 1;

        while (pos_ < src_.size() && (std::isalnum(peek()) || peek() == '_'))
            advance();

        std::string word(src_.substr(start, pos_ - start));
        auto it = KEYWORDS.find(word);
        if (it != KEYWORDS.end())
            return make(it->second, word);

        return make(TokenKind::Ident, word);
    }

    std::vector<Token> Lexer::tokenize() {
        std::vector<Token> tokens;
        while (true) {
            skipWhitespaceAndComments();
            if (pos_ >= src_.size()) {
                tokens.push_back(make(TokenKind::Eof));
                break;
            }
            char c = advance();
            switch (c) {
            case '(':  tokens.push_back(make(TokenKind::LParen));    break;
            case ')':  tokens.push_back(make(TokenKind::RParen));    break;
            case '{':  tokens.push_back(make(TokenKind::LBrace));    break;
            case '}':  tokens.push_back(make(TokenKind::RBrace));    break;
            case '[':  tokens.push_back(make(TokenKind::LBracket));  break;
            case ']':  tokens.push_back(make(TokenKind::RBracket));  break;
            case ',':  tokens.push_back(make(TokenKind::Comma));     break;
            case ':':  tokens.push_back(make(TokenKind::Colon));     break;
            case ';':  tokens.push_back(make(TokenKind::Semicolon)); break;
            case '.':  tokens.push_back(make(TokenKind::Dot));       break;
            case '+':  tokens.push_back(make(TokenKind::Plus));      break;
            case '-':  tokens.push_back(make(TokenKind::Minus));     break;
            case '*':  tokens.push_back(make(TokenKind::Star));      break;
            case '/':  tokens.push_back(make(TokenKind::Slash));     break;
            case '%':  tokens.push_back(make(TokenKind::Percent));   break;
            case '!':  tokens.push_back(make(match('=') ? TokenKind::BangEq : TokenKind::Bang)); break;
            case '<':  tokens.push_back(make(match('=') ? TokenKind::LtEq : TokenKind::Lt));   break;
            case '>':  tokens.push_back(make(match('=') ? TokenKind::GtEq : TokenKind::Gt));   break;
            case '=':
                if (match('='))      tokens.push_back(make(TokenKind::EqEq));
                else if (match('>')) tokens.push_back(make(TokenKind::Arrow));
                else                 tokens.push_back(make(TokenKind::Eq));
                break;
            case '&':
                if (match('&')) tokens.push_back(make(TokenKind::And));
                else tokens.push_back(make(TokenKind::Error, std::string(1, c)));
                break;
            case '|':
                if (match('|')) tokens.push_back(make(TokenKind::Or));
                else tokens.push_back(make(TokenKind::Error, std::string(1, c)));
                break;
            case '"':
                tokens.push_back(readString());
                break;
            default:
                if (std::isdigit(c)) {
                    tokens.push_back(readNumber());
                }
                else if (std::isalpha(c) || c == '_') {
                    tokens.push_back(readIdentOrKeyword());
                }
                else {
                    tokens.push_back(make(TokenKind::Error, std::string(1, c)));
                }
                break;
            }
        }
        return tokens;
    }
} // namespace luma