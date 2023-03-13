#include <catch2/catch_test_macros.hpp>

template <typename T> auto add(T a, T b) { return a + b; }

SCENARIO("single test") {
  GIVEN("Simple add") {
    REQUIRE(add(1, 2) == 3);
    WHEN("test string") { REQUIRE(add(1, 1) == 2); }

    AND_THEN("2 + 3 == 5") {
      REQUIRE(add(2, 3) == 5);
      REQUIRE(add(1 * 2, 3) == 5);
    }
  }
}

TEST_CASE("hello") {
  SECTION("test") { REQUIRE(add(1, 1) == 2); }
}

SCENARIO("Test Name") {
  GIVEN("hello") { REQUIRE(add(1, 1) == 2); }
}