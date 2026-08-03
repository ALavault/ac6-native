/**
 * @file tests/unit/codegen/function_graph_test.cpp
 * @brief Regression tests for overlapping function-entry classification.
 */

#include <catch2/catch_test_macros.hpp>
#include <rex/codegen/function_graph.h>

namespace rex::codegen {

TEST_CASE("Exact overlapping entry remains a function target", "[codegen][function-graph]") {
  FunctionGraph graph;

  auto* outer = graph.addFunction(0x1000, 0x200, FunctionAuthority::CONFIG, true);
  REQUIRE(outer != nullptr);
  outer->discover({{0x1000, 0x200}}, {}, {});

  auto* overlapping =
      graph.addFunction(0x1080, 0x20, FunctionAuthority::CONFIG, true);
  REQUIRE(overlapping != nullptr);
  overlapping->discover({{0x1080, 0x20}}, {}, {});

  // The branch site belongs to the outer function, while the target is both
  // inside its block and an independently declared entry point.
  CHECK(graph.classifyTarget(0x1080, 0x1100, false) == TargetKind::Function);
  CHECK(graph.classifyTarget(0x1000, 0x1100, false) == TargetKind::InternalLabel);
}

}  // namespace rex::codegen
