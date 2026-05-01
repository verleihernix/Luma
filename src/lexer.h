#pragma once
#include <string>
#include <vector>
#include <string_view>

namespace luma {
	enum class TokenKind {
		Number, String, True, False, Null,

		Ident,
		Let, Fn, Return, If, Else, While, For, In,
		
		LParen, RParen, LBrace, RBrace, LBracket, RBracket,
		Comma, Colon, Semicolon, Dot,

		Plus, Minus, Star, Slash, Percent,
		Eq, EqEq, BangEq, Lt, LtEq, Gt, GtEq,
		Bang, And, Or,
		Arrow, // =>

		Eof, Error
	};

	struct Token {
		TokenKind    kind;
		std::string  value;
		int          line;
		int          col;
	};

	class Lexer {
	public:
		explicit Lexer(std::string_view src);
		std::vector<Token> tokenize();
	private:
		char advance();
		char peek(int offset = 0) const;
		bool match(char expected);
		void skipWhitespaceAndComments();
		Token readString();
		Token readNumber();
		Token readIdentOrKeyword();
		Token make(TokenKind k, std::string val = {}) const;

		std::string_view src_;
		size_t pos_ = 0;
		int line_ = 1;
		int col_ = 1;
	};
} // namespace luma