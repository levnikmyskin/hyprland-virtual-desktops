#include "layout_utils.hpp"

int main() {
    if (deskIdFromWorkspaceRaw(-1, 2) != 1)
        return 1;

    if (deskIdFromWorkspaceRaw(0, 2) != 1)
        return 2;

    if (deskIdFromWorkspaceRaw(1, 2) != 1)
        return 3;

    if (deskIdFromWorkspaceRaw(2, 2) != 1)
        return 4;

    if (deskIdFromWorkspaceRaw(3, 2) != 2)
        return 5;

    if (deskIdFromWorkspaceRaw(4, 2) != 2)
        return 6;

    if (deskIdFromWorkspaceRaw(5, 2) != 3)
        return 7;

    if (deskIdFromWorkspaceRaw(6, 3) != 2)
        return 8;

    if (deskIdFromWorkspaceRaw(7, 3) != 3)
        return 9;

    if (deskIdFromWorkspaceRaw(8, 3) != 3)
        return 10;

    if (deskIdFromWorkspaceRaw(42, 5) != 9)
        return 11;

    if (deskIdFromWorkspaceRaw(10, 0) != 1)
        return 12;

    return 0;
}
