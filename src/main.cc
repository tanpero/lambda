#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
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

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (position < input.length()) {
            skipWhitespace();
            if (position >= input.length()) break;

            Char current = input[position++];
            if (current == "λ") {
                tokens.emplace_back(TokenType::LAMBDA);
            } else if (current == ".") {
                tokens.emplace_back(TokenType::DOT);
            } else if (current == "(") {
                tokens.emplace_back(TokenType::LPAREN);
            } else if (current == ")") {
                tokens.emplace_back(TokenType::RPAREN);
            } else if (!isspace(current.toCodepoint()) && !isdigit(current.toCodepoint())) {
                tokens.emplace_back(TokenType::VARIABLE, current);
            } else {
                throw std::runtime_error("Unexpected character encountered");
            }
        }
        tokens.emplace_back(TokenType::END); // Add an END token at the end
        return tokens;
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
    Parser(const std::vector<Token>& tokens) 
      : tokens(tokens), currentPosition(0), currentToken(tokens[currentPosition]) {
    }

    ExprPtr parse() {
        return parseExpression();
    }

private:
    std::vector<Token> tokens;
    size_t currentPosition;
    Token currentToken;

    Token nextToken() {
        if (currentPosition < tokens.size() - 1) { // Ensure position is with bounds
            currentToken = tokens[++currentPosition];
        }
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

            // Multi-parameters lambda may be considered as nested single-parameter lambda
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


// Check if a variable occurs in expression.
bool occursIn(const String& varName, ExprPtr expr) {
    if (auto var = std::dynamic_pointer_cast<Variable>(expr)) {
        return var->name == varName;
    } else if (auto abstraction = std::dynamic_pointer_cast<Abstraction>(expr)) {
        return abstraction->param == varName || occursIn(varName, abstraction->body);
    } else if (auto application = std::dynamic_pointer_cast<Application>(expr)) {
        return occursIn(varName, application->func) || occursIn(varName, application->arg);
    }
    return false;
}

// Rename variables to avoid from naming conflict.
String freshName(const String& base, ExprPtr context) {
    String newName = base;
    size_t i = 0;
    while (occursIn(newName, context)) {
        newName = base + std::to_string(i++);
    }
    return newName;
}

// α-Convert: Change the names of parameters to avoid from conflict
ExprPtr alphaConvert(ExprPtr expr, const String& oldVar, const String& newVar) {
    if (auto var = std::dynamic_pointer_cast<Variable>(expr)) {
        if (var->name == oldVar) {
            return std::make_shared<Variable>(newVar);
        } else {
            return var;
        }
    } else if (auto abstraction = std::dynamic_pointer_cast<Abstraction>(expr)) {
        if (abstraction->param == oldVar) {
            return std::make_shared<Abstraction>(newVar, alphaConvert(abstraction->body, oldVar, newVar));
        } else {
            return std::make_shared<Abstraction>(abstraction->param, alphaConvert(abstraction->body, oldVar, newVar));
        }
    } else if (auto application = std::dynamic_pointer_cast<Application>(expr)) {
        return std::make_shared<Application>(
            alphaConvert(application->func, oldVar, newVar),
            alphaConvert(application->arg, oldVar, newVar)
        );
    }
    throw std::runtime_error("Unrecognized expression in alpha conversion");
}

// Replace the variable `varName` with `value` in expression.
ExprPtr substitute(ExprPtr expr, const String& varName, ExprPtr value) {
    if (auto var = std::dynamic_pointer_cast<Variable>(expr)) {
        if (var->name == varName) {
            return value;
        } else {
            return var;
        }
    } else if (auto abstraction = std::dynamic_pointer_cast<Abstraction>(expr)) {
        if (abstraction->param == varName) {
            return abstraction;

        // Prevent free variables from being captured.
        } else if (occursIn(abstraction->param, value)) {
            String newParamName = freshName(abstraction->param, value);
            ExprPtr newBody = alphaConvert(abstraction->body, abstraction->param, newParamName);
            return std::make_shared<Abstraction>(newParamName, substitute(newBody, varName, value));
        } else {
            return std::make_shared<Abstraction>(
                abstraction->param,
                substitute(abstraction->body, varName, value)
            );
        }
    } else if (auto application = std::dynamic_pointer_cast<Application>(expr)) {
        return std::make_shared<Application>(
            substitute(application->func, varName, value),
            substitute(application->arg, varName, value)
        );
    }
    throw std::runtime_error("Unrecognized expression in substitution");
}

// β-Reduce: Obtain the replaced steps.
ExprPtr betaReduceStep(ExprPtr expr) {
    if (auto application = std::dynamic_pointer_cast<Application>(expr)) {
        if (auto abstraction = std::dynamic_pointer_cast<Abstraction>(application->func)) {
            std::cout << Char{ 0x21aa } << " β-reduce: " << abstraction->param << " <- " << application->arg->toString() << std::endl;
            return substitute(abstraction->body, abstraction->param, application->arg);
        } else {
            return std::make_shared<Application>(
                betaReduceStep(application->func),
                betaReduceStep(application->arg)
            );
        }
    } else if (auto abstraction = std::dynamic_pointer_cast<Abstraction>(expr)) {
        return std::make_shared<Abstraction>(abstraction->param, betaReduceStep(abstraction->body));
    }
    return expr;
}

// Determine if the expression has been reduced to its final form.
bool isReduced(ExprPtr expr) {
    if (auto application = std::dynamic_pointer_cast<Application>(expr)) {
        return !(std::dynamic_pointer_cast<Abstraction>(application->func)) &&
               isReduced(application->func) && isReduced(application->arg);
    } else if (auto abstraction = std::dynamic_pointer_cast<Abstraction>(expr)) {
        return isReduced(abstraction->body);
    } else if (std::dynamic_pointer_cast<Variable>(expr)) {
        return true;
    }
    return false;
}

// Reduce expression to the final form.
ExprPtr betaReduce(ExprPtr expr) {
    while (!isReduced(expr)) {
        expr = betaReduceStep(expr);
    }
    std::cout << "done." << std::endl;
    return expr;
}

struct Result {
    String value;
    bool isOk;
};

// Evaluate and β-Reduce the source expression.
Result evaluate(const String& input) {
    try {
        Lexer lexer(input);
        Parser parser(lexer.tokenize());
        ExprPtr expression = parser.parse();
        ExprPtr reducedExpression = betaReduce(expression);
        return { reducedExpression->toString(), true };
    } catch (const std::exception& e) {
        return { String("Error: ") + e.what(), false };
    }
}

struct BindingEntry {
    String name;
    String expr;
};

std::vector<BindingEntry> globalMapping;

enum class InputType {
    Expression,
    Binding, InvalidBinding,
    Assertion, InvalidAssertion,
};

std::string trim(const std::string& str) {
    std::string s = str;
    s.erase(0, std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }) - s.begin());
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base() - s.begin(), s.length());
    std::replace(s.begin(), s.end(), ' ', '-');
    return s;
}

InputType processBinding(const std::string& input) {
    std::string trimmedInput = input.substr(input.find_first_not_of(' '));
    if (trimmedInput.substr(0, 4) != "let ") {
        return InputType::Expression;
    }
    size_t wordStart = 4;
    size_t wordEnd = trimmedInput.find('=', wordStart);
    if (wordEnd == std::string::npos) {
        return InputType::InvalidBinding;
    }
    String word = trim(trimmedInput.substr(wordStart, wordEnd - wordStart));
    String expression = trimmedInput.substr(wordEnd + 1);
    globalMapping.push_back({ word, expression });
    return InputType::Binding;
}


String interpret(const String& input) {
    InputType inputType = processBinding(input.toUTF8());
    if (inputType == InputType::Binding) {
        auto entry = globalMapping.back();
        Result result = evaluate(entry.expr);
        if (!result.isOk) {
            globalMapping.pop_back();
            return result.value;
        }
        else return "<" + entry.name + "> " + result.value;
    }
    else if (inputType == InputType::Expression) {
        Result result = evaluate(entry.expr);
        return result.value;
    }
    else return "Invalid Syntax";
}

int main(int argc, char* argv[]) {
    std::string input;
    while (true) {
        input = readline("λ> ");
        if (input.empty()) if ((input = readline("λ> ")).empty()) break;
        size_t pos = input.find('\\');
        while (pos != std::string::npos) {
            input.replace(pos, 1, "λ");
            pos = input.find('\\', pos + 1);
        }
        add_history(input.c_str());
        std::cout << " - " << input << " - \n" << interpret(String{ input }) << "\n" << std::endl;
    }
    return 0;
}

