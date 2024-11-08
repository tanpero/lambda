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

class Expr {
public:
    virtual ~Expr() = default;
    virtual String toString() const = 0;
};

using ExprPtr = std::shared_ptr<Expr>;

class Variable : public Expr {
public:
    Variable(const String& name) : name(name) {}
    String toString() const override {
        return name;
    }
    String name;
};

class Abstraction : public Expr {
public:
    Abstraction(const String& param, ExprPtr body) : param(param), body(body) {}
    String toString() const override {
        return "λ" + param + "." + body->toString();
    }
    String param;
    ExprPtr body;
};

class Application : public Expr {
public:
    Application(ExprPtr func, ExprPtr arg) : func(func), arg(arg) {}
    String toString() const override {
        return "(" + func->toString() + " " + arg->toString() + ")";
    }
    ExprPtr func;
    ExprPtr arg;
};

class Parser {
public:
    Parser(Lexer& lexer) : lexer(lexer), currentToken(lexer.nextToken()) {

    }

    ExprPtr parse() {
        return parseExpression();
    }

private:
    Lexer& lexer;
    Token currentToken;

    Token nextToken() {
        currentToken = lexer.nextToken();
        return currentToken;
    }

    ExprPtr parseExpression() {
        if (currentToken.type == TokenType::LAMBDA) {
            nextToken(); // skip LAMBDA

            // To collect all parameters.
            std::vector<String> parameters;
            while (currentToken.type == TokenType::VARIABLE) {
                parameters.push_back(currentToken.value);
                nextToken(); // skip VARIABLE
            }

            if (currentToken.type != TokenType::DOT) {
                throw std::runtime_error("Expected '.' after lambda parameters");
            }
            nextToken(); // skip DOT

            ExprPtr body = parseExpression();

            // 多参数 lambda 被视为嵌套的单参数 lambda
            for (auto it = parameters.rbegin(); it != parameters.rend(); ++it) {
                body = std::make_shared<Abstraction>(*it, body);
            }

            return body;
        } else {
            return parseApplication();
        }
    }

    ExprPtr parseApplication() {
        ExprPtr expr = parseTerm();
        while (currentToken.type == TokenType::VARIABLE || currentToken.type == TokenType::LPAREN) {
            ExprPtr right = parseTerm();
            expr = std::make_shared<Application>(expr, right);
        }
        return expr;
    }

    ExprPtr parseTerm() {
        if (currentToken.type == TokenType::VARIABLE) {
            String varName = currentToken.value;
            nextToken(); // skip VARIABLE
            return std::make_shared<Variable>(varName);
        } else if (currentToken.type == TokenType::LPAREN) {
            nextToken(); // skip LPAREN
            ExprPtr expr = parseExpression();
            if (currentToken.type != TokenType::RPAREN) {
                throw std::runtime_error("Expected closing parenthesis");
            }
            nextToken(); // skip RPAREN
            return expr;
        } else {
            throw std::runtime_error("Unexpected term");
        }
    }
};

