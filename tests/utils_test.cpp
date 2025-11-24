#include "layout_utils.hpp"
#include "test_helpers.hpp"

int main() {
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(-1, 2), 1);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(0, 2), 1);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(1, 2), 1);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(2, 2), 1);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(3, 2), 2);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(4, 2), 2);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(5, 2), 3);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(6, 3), 2);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(7, 3), 3);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(8, 3), 3);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(42, 5), 9);
    TEST_EXPECT_EQ(deskIdFromWorkspaceRaw(10, 0), 1);
    TEST_ASSERT_OK();
}
