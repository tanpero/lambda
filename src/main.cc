#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <readline/readline.h>
#include <readline/history.h>
#include "string.hh"

enum class TokenType { VARIABLE, LAMBDA, DOT, LPAREN, RPAREN, END };

struct Token {
    TokenType type;
    Char value;
    Token(TokenType t, const Char& v = "") : type(t), value(v) {}
};

class Lexer {
public:
    Lexer(const String& input) : input(input), position(0) {}
    
    Token nextToken() {
        skipWhitespace();
        
        if (position >= input.length()) return Token(TokenType::END);
        
        Char current = input[position++];
        
        if(current == "λ") {
            return Token(TokenType::LAMBDA);
        }
        else if(current == ".") {
            return Token(TokenType::DOT);
        }
        else if(current == "(") {
            return Token(TokenType::LPAREN);
        }
        else if (current == ")") {
            return Token(TokenType::RPAREN);
        }
        else if (!isspace(current.toCodepoint() && !isdigit(current.toCodepoint()))) {
            return Token(TokenType::VARIABLE, current);
        }
        
        throw std::runtime_error("Unexpected character encountered");
    }

private:
    String input;
    size_t position;

    void skipWhitespace() {
        while (position < input.length() && isspace(input[position].toCodepoint())) {
            ++position;
        }
    }
};

