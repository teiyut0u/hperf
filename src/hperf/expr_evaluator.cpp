#include "hperf/expr_evaluator.h"

#include <cctype>
#include <iostream>
#include <stdexcept>

namespace {

enum class TokenType { NUMBER,
                       IDENT,
                       PLUS,
                       MINUS,
                       STAR,
                       SLASH,
                       LPAREN,
                       RPAREN,
                       END };

struct Token {
  TokenType type;
  double number = 0.0;
  std::string ident;
};

class Lexer {
 public:
  explicit Lexer(const std::string& input) : input_(input), pos_(0) {}

  Token next() {
    // Skip whitespace
    while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_;

    if (pos_ >= input_.size()) return {TokenType::END};

    char c = input_[pos_];

    if (c == '+') {
      ++pos_;
      return {TokenType::PLUS};
    }
    if (c == '-') {
      ++pos_;
      return {TokenType::MINUS};
    }
    if (c == '*') {
      ++pos_;
      return {TokenType::STAR};
    }
    if (c == '/') {
      ++pos_;
      return {TokenType::SLASH};
    }
    if (c == '(') {
      ++pos_;
      return {TokenType::LPAREN};
    }
    if (c == ')') {
      ++pos_;
      return {TokenType::RPAREN};
    }

    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
      size_t start = pos_;
      while (pos_ < input_.size() &&
             (std::isdigit(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '.')) {
        ++pos_;
      }
      Token tok;
      tok.type = TokenType::NUMBER;
      tok.number = std::stod(input_.substr(start, pos_ - start));
      return tok;
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      size_t start = pos_;
      while (pos_ < input_.size() &&
             (std::isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) {
        ++pos_;
      }
      Token tok;
      tok.type = TokenType::IDENT;
      tok.ident = input_.substr(start, pos_ - start);
      return tok;
    }

    throw std::runtime_error(std::string("unexpected character: ") + c);
  }

  // Peek at next token without consuming it
  Token peek() {
    size_t saved = pos_;
    Token tok = next();
    pos_ = saved;
    return tok;
  }

 private:
  const std::string& input_;
  size_t pos_;
};

class Parser {
 public:
  Parser(const std::string& expr, const std::unordered_map<std::string, double>& vars)
      : lexer_(expr), vars_(vars) {
    current_ = lexer_.next();
  }

  double parse_expr() {
    double val = parse_term();
    while (current_.type == TokenType::PLUS || current_.type == TokenType::MINUS) {
      bool plus = (current_.type == TokenType::PLUS);
      consume();
      double rhs = parse_term();
      val = plus ? val + rhs : val - rhs;
    }
    return val;
  }

 private:
  Lexer lexer_;
  const std::unordered_map<std::string, double>& vars_;
  Token current_;

  void consume() { current_ = lexer_.next(); }

  double parse_term() {
    double val = parse_factor();
    while (current_.type == TokenType::STAR || current_.type == TokenType::SLASH) {
      bool mul = (current_.type == TokenType::STAR);
      consume();
      double rhs = parse_factor();
      if (mul) {
        val = val * rhs;
      } else {
        if (rhs == 0.0) {
          std::cerr << "Warning: division by zero in expression, substituting 0\n";
          val = 0.0;
        } else {
          val = val / rhs;
        }
      }
    }
    return val;
  }

  double parse_factor() {
    // Unary minus
    if (current_.type == TokenType::MINUS) {
      consume();
      return -parse_factor();
    }

    // Parenthesised expression
    if (current_.type == TokenType::LPAREN) {
      consume();
      double val = parse_expr();
      if (current_.type != TokenType::RPAREN)
        throw std::runtime_error("expected closing parenthesis");
      consume();
      return val;
    }

    // Numeric literal
    if (current_.type == TokenType::NUMBER) {
      double val = current_.number;
      consume();
      return val;
    }

    // Variable
    if (current_.type == TokenType::IDENT) {
      auto it = vars_.find(current_.ident);
      if (it == vars_.end()) {
        std::cerr << "Warning: unknown variable '" << current_.ident
                  << "' in expression, substituting 0\n";
        consume();
        return 0.0;
      }
      double val = it->second;
      consume();
      return val;
    }

    throw std::runtime_error("unexpected token in expression");
  }
};

}  // namespace

double ExprEvaluator::evaluate(const std::string& expr,
                               const std::unordered_map<std::string, double>& vars) {
  try {
    Parser parser(expr, vars);
    return parser.parse_expr();
  } catch (const std::exception& e) {
    std::cerr << "Error evaluating expression '" << expr << "': " << e.what() << "\n";
    return 0.0;
  }
}
