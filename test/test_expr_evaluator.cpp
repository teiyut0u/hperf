#include <gtest/gtest.h>

#include <unordered_map>

#include "hperf/expr_evaluator.h"

using Vars = std::unordered_map<std::string, double>;

// ------------------------------------------------------------------ Literals
TEST(ExprEvaluator, IntegerLiteral) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("42", {}), 42.0);
}

TEST(ExprEvaluator, FloatLiteral) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("3.14", {}), 3.14);
}

// ------------------------------------------------------------------ Arithmetic
TEST(ExprEvaluator, Addition) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("1 + 2", {}), 3.0);
}

TEST(ExprEvaluator, Subtraction) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("5 - 3", {}), 2.0);
}

TEST(ExprEvaluator, Multiplication) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("3 * 4", {}), 12.0);
}

TEST(ExprEvaluator, Division) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("10 / 4", {}), 2.5);
}

TEST(ExprEvaluator, OperatorPrecedence) {
  // Multiplication before addition
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("1 + 2 * 3", {}), 7.0);
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("2 * 3 + 1", {}), 7.0);
}

TEST(ExprEvaluator, Parentheses) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("(1 + 2) * 3", {}), 9.0);
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("10 / (2 + 3)", {}), 2.0);
}

TEST(ExprEvaluator, UnaryMinus) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("-5", {}), -5.0);
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("10 + -3", {}), 7.0);
}

// ------------------------------------------------------------------ Variables
TEST(ExprEvaluator, SingleVariable) {
  Vars vars = {{"cpu_cycles", 1000.0}};
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("cpu_cycles", vars), 1000.0);
}

TEST(ExprEvaluator, VariableArithmetic) {
  Vars vars = {{"cpu_cycles", 2000.0}, {"inst_retired", 1000.0}};
  // IPC
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("inst_retired / cpu_cycles", vars), 0.5);
}

TEST(ExprEvaluator, UnknownVariableReturnsZero) {
  // Unknown variable should not crash and should return 0.0
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("unknown_event", {}), 0.0);
}

// ------------------------------------------------------------------ Edge cases
TEST(ExprEvaluator, DivisionByZeroReturnsZero) {
  Vars vars = {{"a", 1.0}, {"b", 0.0}};
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("a / b", vars), 0.0);
}

TEST(ExprEvaluator, DivisionByZeroLiteral) {
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("5 / 0", {}), 0.0);
}

// ------------------------------------------------------------------ Real-world metric expressions
// These mirror the actual expressions used in config/cpu_*.toml

TEST(ExprEvaluator, MetricIPC) {
  Vars vars = {{"inst_retired", 3000.0}, {"cpu_cycles", 1000.0}};
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("inst_retired / cpu_cycles", vars), 3.0);
}

TEST(ExprEvaluator, MetricBranchMissRate) {
  Vars vars = {{"br_mis_pred_retired", 100.0}, {"br_retired", 1000.0}};
  EXPECT_NEAR(ExprEvaluator::evaluate("br_mis_pred_retired / br_retired", vars), 0.1, 1e-9);
}

TEST(ExprEvaluator, MetricFrequencyGHz) {
  // cpu_cycles * cntfrq / (cnt_cycles * 1000000000)
  // With cntfrq = 19.2e6, cnt_cycles = 192, cpu_cycles = 100 → 100 * 19.2e6 / (192 * 1e9) = 0.01 GHz
  Vars vars = {{"cpu_cycles", 100.0},
               {"cntfrq", 19200000.0},
               {"cnt_cycles", 192.0}};
  double result = ExprEvaluator::evaluate(
      "cpu_cycles * cntfrq / (cnt_cycles * 1000000000)", vars);
  EXPECT_NEAR(result, 0.01, 1e-9);
}

TEST(ExprEvaluator, MetricL1DCacheMissRate) {
  Vars vars = {{"l1d_cache_refill", 50.0}, {"l1d_cache", 1000.0}};
  EXPECT_DOUBLE_EQ(ExprEvaluator::evaluate("l1d_cache_refill / l1d_cache", vars), 0.05);
}

TEST(ExprEvaluator, MetricMemoryLatency) {
  // bus_access_rd_cycles / bus_access_rd
  Vars vars = {{"bus_access_rd_cycles", 400.0}, {"bus_access_rd", 100.0}};
  EXPECT_DOUBLE_EQ(
      ExprEvaluator::evaluate("bus_access_rd_cycles / bus_access_rd", vars), 4.0);
}
