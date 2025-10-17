#include "layout_utils.hpp"
#include "test_helpers.hpp"
#include <cstddef>

int main() {
    for (std::size_t monitors = 1; monitors <= 8; ++monitors) {
        for (long ws = -monitors * 2; ws <= static_cast<long>(monitors) * 20; ++ws) {
            int desk = deskIdFromWorkspaceRaw(ws, monitors);
            TEST_EXPECT_TRUE(desk >= 1);
            long deskBase = static_cast<long>(desk - 1) * static_cast<long>(monitors) + 1;
            if (ws >= 1) {
                TEST_EXPECT_TRUE(ws >= deskBase);
                TEST_EXPECT_TRUE(ws < deskBase + static_cast<long>(monitors));
            }
        }
    }

    TEST_ASSERT_OK();
}
