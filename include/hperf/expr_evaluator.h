#pragma once

#include <string>
#include <unordered_map>

/**
 * @brief Evaluates simple arithmetic expressions with named variables.
 *
 * Supported syntax:
 *   - Four arithmetic operations: +  -  *  /
 *   - Parentheses for grouping
 *   - Unary minus
 *   - Integer and decimal numeric literals
 *   - Variable names (letters, digits, underscores; must start with a letter or underscore)
 *
 * On unknown variable or division by zero, the affected sub-expression evaluates to 0.0.
 * On parse error, returns 0.0 and prints a diagnostic to stderr.
 */
class ExprEvaluator {
 public:
  static double evaluate(const std::string& expr,
                         const std::unordered_map<std::string, double>& vars);
};
