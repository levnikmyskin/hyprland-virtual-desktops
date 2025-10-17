#include "FocusMemory.hpp"
#include <memory>

int main() {
    FocusMemory<int> tracker;

    auto validatorAlways = [](const std::shared_ptr<int>&) { return true; };

    if (tracker.recall(validatorAlways))
        return 1; // unexpected initial value

    auto value = std::make_shared<int>(42);
    tracker.remember(value);

    auto recalled = tracker.recall(validatorAlways);
    if (!recalled || *recalled != 42)
        return 2;

    tracker.forget(value);
    if (tracker.recall(validatorAlways))
        return 3;

    auto otherValue = std::make_shared<int>(7);
    tracker.remember(otherValue);
    auto rejected = tracker.recall([](const std::shared_ptr<int>& v) { return v && *v == 42; });
    if (rejected)
        return 4;

    auto check = tracker.recall(validatorAlways);
    if (!check || *check != 7)
        return 5;

    check.reset();
    otherValue.reset();
    if (tracker.recall(validatorAlways))
        return 6;

    FocusMemory<int> validatorTest;
    auto initial = std::make_shared<int>(3);
    validatorTest.remember(initial);
    auto onlyEven = [](const std::shared_ptr<int>& v) { return v && (*v % 2 == 0); };
    if (validatorTest.recall(onlyEven))
        return 7;

    auto even = std::make_shared<int>(4);
    validatorTest.remember(even);
    auto evenResult = validatorTest.recall(onlyEven);
    if (!evenResult || *evenResult != 4)
        return 8;

    validatorTest.forget(even);
    if (validatorTest.recall(validatorAlways))
        return 9;

    return 0;
}
