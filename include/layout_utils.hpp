#pragma once

#include <cstddef>

inline int deskIdFromWorkspaceRaw(long workspaceId, std::size_t monitorCount) {
    if (workspaceId <= 0 || monitorCount == 0)
        return 1;
    return static_cast<int>((workspaceId - 1) / static_cast<long>(monitorCount)) + 1;
}

