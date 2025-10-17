#include "FocusMemory.hpp"
#include "test_helpers.hpp"
#include <memory>

int main() {
    FocusMemory<int> tracker;

    auto validatorAlways = [](const std::shared_ptr<int>&) { return true; };

    TEST_EXPECT_FALSE(tracker.recall(validatorAlways));

    auto value = std::make_shared<int>(42);
    tracker.remember(value);

    auto recalled = tracker.recall(validatorAlways);
    TEST_EXPECT_TRUE(recalled);
    TEST_EXPECT_EQ(*recalled, 42);

    tracker.forget(value);
    TEST_EXPECT_FALSE(tracker.recall(validatorAlways));

    auto otherValue = std::make_shared<int>(7);
    tracker.remember(otherValue);
    auto rejected = tracker.recall([](const std::shared_ptr<int>& v) { return v && *v == 42; });
    TEST_EXPECT_FALSE(rejected);

    auto check = tracker.recall(validatorAlways);
    TEST_EXPECT_TRUE(check);
    TEST_EXPECT_EQ(*check, 7);

    check.reset();
    otherValue.reset();
    TEST_EXPECT_FALSE(tracker.recall(validatorAlways));

    FocusMemory<int> validatorTest;
    auto initial = std::make_shared<int>(3);
    validatorTest.remember(initial);
    auto onlyEven = [](const std::shared_ptr<int>& v) { return v && (*v % 2 == 0); };
    TEST_EXPECT_FALSE(validatorTest.recall(onlyEven));

    auto even = std::make_shared<int>(4);
    validatorTest.remember(even);
    auto evenResult = validatorTest.recall(onlyEven);
    TEST_EXPECT_TRUE(evenResult);
    TEST_EXPECT_EQ(*evenResult, 4);

    validatorTest.forget(even);
    TEST_EXPECT_FALSE(validatorTest.recall(validatorAlways));

    validatorTest.remember(std::make_shared<int>(99));
    validatorTest.reset();
    TEST_EXPECT_FALSE(validatorTest.recall(validatorAlways));

    TEST_ASSERT_OK();
}
