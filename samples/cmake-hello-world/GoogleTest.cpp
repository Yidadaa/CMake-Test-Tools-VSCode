#include "gtest/gtest.h"

int add(int a, int b) { return a + b; }

TEST(simple, simple_add_func) {
  ASSERT_EQ(add(1, 2), 3);
  ASSERT_LE(add(1, 1), 3);
}

class TestFixture : public ::testing::Test {
public:
  int a = 0;
  int b = 1;
};

TEST_F(TestFixture, simple_add_with_fixture) {
  ASSERT_NE(add(a, b), 3);
  ASSERT_EQ(add(a, b), 1);
}

class TestAdd : public testing::TestWithParam<int> {
  void SetUp() override { this->a_ = GetParam(); }

protected:
  int a_;
};

TEST_P(TestAdd, simple_add_with_param) {
  const auto x = a_;
  ASSERT_GT(add(x, x), 1);
}

INSTANTIATE_TEST_SUITE_P(one_or_two, TestAdd, testing::Values(1, 2));