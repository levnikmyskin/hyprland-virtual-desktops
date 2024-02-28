#include "VirtualDeskManager.hpp"
#include <hyprland/src/Compositor.hpp>
#include <format>
#include <ranges>

VirtualDeskManager::VirtualDeskManager() {
    this->conf = RememberLayoutConf::size;
    getOrCreateVdesk(1);
}

const std::shared_ptr<VirtualDesk>& VirtualDeskManager::activeVdesk() {
    return vdesksMap[m_activeDeskKey];
}

void VirtualDeskManager::changeActiveDesk(std::string& arg, bool apply) {
    int vdeskId;
    try {
        vdeskId = std::stoi(arg);
    } catch (std::exception const& ex) { vdeskId = getDeskIdFromName(arg); }

    changeActiveDesk(vdeskId, apply);
}

void VirtualDeskManager::changeActiveDesk(int vdeskId, bool apply) {
    if (vdeskId == activeVdesk()->id) {
        cycleWorkspaces();
        return;
    }

    getOrCreateVdesk(vdeskId);
    lastDesk        = activeVdesk()->id;
    m_activeDeskKey = vdeskId;
    if (apply)
        applyCurrentVDesk();
}

void VirtualDeskManager::lastVisitedDesk() {
    if (lastDesk == -1) {
        printLog("There's no last desk");
        return;
    }
    changeActiveDesk(lastDesk, true);
}

void VirtualDeskManager::prevDesk(bool backwardCycle) {
    changeActiveDesk(prevDeskId(backwardCycle), true);
}

void VirtualDeskManager::nextDesk(bool cycle) {
    changeActiveDesk(nextDeskId(cycle), true);
}

void VirtualDeskManager::applyCurrentVDesk() {
    if (currentlyEnabledMonitors().size() == 0) {
        printLog("There are no monitors!");
        return;
    }
    if (isVerbose())
        printLog("applying vdesk" + activeVdesk()->name);
    auto        currentMonitor   = getCurrentMonitor();
    auto        layout           = activeVdesk()->activeLayout(conf);
    CWorkspace* focusedWorkspace = nullptr;
    for (auto [lmon, workspaceId] : layout) {
        CMonitor* mon = g_pCompositor->getMonitorFromID(lmon->ID);
        if (!lmon || !lmon->m_bEnabled) {
            printLog("One of the monitors in the vdesk went bonkers...Will try to find another one");
            mon = activeVdesk()->deleteInvalidMonitor(lmon);
            // Big F, we can't do much here
            if (!mon) {
                printLog("There is no enabled monitor at all. I give up :)");
                return;
            }
        }
        CWorkspace* workspace = g_pCompositor->getWorkspaceByID(workspaceId);
        if (!workspace) {
            printLog("Creating workspace " + std::to_string(workspaceId));
            workspace = g_pCompositor->createNewWorkspace(workspaceId, mon->ID);
        }

        if (workspace->m_iMonitorID != mon->ID)
            g_pCompositor->moveWorkspaceToMonitor(workspace, currentMonitor);

        // Hack: we change the workspace on the current monitor as our last operation,
        // so that we also automatically focus it
        if (currentMonitor && mon->ID == currentMonitor->ID) {
            focusedWorkspace = workspace;
            continue;
        }
        mon->changeWorkspace(workspace, false);
    }
    if (currentMonitor && focusedWorkspace)
        currentMonitor->changeWorkspace(focusedWorkspace, false);
}

int VirtualDeskManager::moveToDesk(std::string& arg, int vdeskId) {
    // TODO: this should be improved:
    //   1. if there's an empty workspace on the specified vdesk, we should move the window there;
    //   2. we should give a way to specify on which workspace to move the window. It'd be best if user could specify 1,2,3
    //      and we move the window to the first, second or third monitor on the vdesk (from left to right)
    if (arg == MOVETODESK_DISPATCH_STR) {
        // TODO: notify about missing args
        return -1;
    }

    if (vdeskId < 1) {
        auto vdeskName = parseMoveDispatch(arg);
        try {
            vdeskId = std::stoi(vdeskName);
        } catch (std::exception& _) { vdeskId = getDeskIdFromName(vdeskName); }
    }

    if (isVerbose())
        printLog("creating new vdesk with id " + std::to_string(vdeskId));

    auto vdesk = getOrCreateVdesk(vdeskId);

    // just take the first workspace wherever in the layout
    auto        wid = vdesk->activeLayout(conf).begin()->second;

    std::string moveCmd;
    if (arg == "") {
        moveCmd = std::to_string(wid);
    } else {
        moveCmd = std::to_string(wid) + "," + arg;
    }

    HyprlandAPI::invokeHyprctlCommand("dispatch", "movetoworkspacesilent " + moveCmd);
    return vdeskId;
}

int VirtualDeskManager::getDeskIdFromName(const std::string& name, bool createIfNotFound) {
    int  max_key = -1;
    bool found   = false;
    int  vdesk   = -1;
    for (auto const& [key, val] : vdeskNamesMap) {
        if (val == name) {
            vdesk = key;
            found = true;
            break;
        }
        if (key > max_key)
            max_key = key;
    }
    if (!found && createIfNotFound) {
        vdesk = max_key + 1;
        getOrCreateVdesk(vdesk);
    }
    return vdesk;
}

void VirtualDeskManager::loadLayoutConf() {
    // Having the possibility to change this at any given time
    // poses just too many issues to think about for now.
    // Maybe in a future release :)
    if (confLoaded)
        return;
    static auto* const PREMEMBER_LAYOUT = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, REMEMBER_LAYOUT_CONF);
    conf                                = layoutConfFromString(*PREMEMBER_LAYOUT);
    confLoaded                          = true;
}

void VirtualDeskManager::cycleWorkspaces() {
    static auto* const PCYCLEWORKSPACES = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, CYCLEWORKSPACES_CONF);
    if (!**PCYCLEWORKSPACES)
        return;

    auto      n_monitors     = g_pCompositor->m_vMonitors.size();
    CMonitor* currentMonitor = g_pCompositor->m_pLastMonitor;

    // TODO: implement for more than two monitors as well.
    // This probably requires to compute monitors position
    // in order to consistently move left/right or up/down.
    if (n_monitors == 2) {
        int  other    = g_pCompositor->m_vMonitors[0]->ID == currentMonitor->ID;
        auto otherMon = g_pCompositor->m_vMonitors[other].get();
        g_pCompositor->swapActiveWorkspaces(currentMonitor, otherMon);

        auto currentWorkspace = g_pCompositor->getWorkspaceByID(currentMonitor->activeWorkspace);
        auto otherWorkspace   = g_pCompositor->getWorkspaceByID(otherMon->activeWorkspace);
        activeVdesk()->changeWorkspaceOnMonitor(currentWorkspace->m_iID, currentMonitor);
        activeVdesk()->changeWorkspaceOnMonitor(otherWorkspace->m_iID, otherMon);
    } else if (n_monitors > 2) {
        printLog("Cycling workspaces is not yet implemented for more than 2 monitors."
                 "\nIf you would like to have this feature, open an issue on virtual-desktops github repo, or even "
                 "better, open a PR :)");
    }
}

void VirtualDeskManager::deleteInvalidMonitorsOnAllVdesks(const CMonitor* monitor) {
    for (const auto& [_, vdesk] : vdesksMap) {
        // recompute active layout
        vdesk->activeLayout(conf, monitor);
        if (monitor)
            printLog(std::format("Deleting monitor with exclude {}", monitor->szName));
        vdesk->deleteInvalidMonitor(monitor);
    }
}

void VirtualDeskManager::deleteInvalidMonitorsOnAllVdesks() {
    for (const auto& [_, vdesk] : vdesksMap) {
        // recompute active layout
        vdesk->activeLayout(conf);
        vdesk->deleteInvalidMonitorsOnActiveLayout();
    }
}

void VirtualDeskManager::resetAllVdesks() {
    for (const auto& [_, vdesk] : vdesksMap) {
        vdesk->resetLayout();
    }
}

void VirtualDeskManager::resetVdesk(const std::string& arg) {
    int vdeskId;
    try {
        vdeskId = std::stoi(arg);
    } catch (std::exception const& ex) { vdeskId = getDeskIdFromName(arg, false); }

    if (vdeskId == -1) {
        printLog("Reset vdesk: " + arg + " not found", LogLevel::WARN);
        return;
    }

    if (!vdesksMap.contains(vdeskId)) {
        printLog("Reset vdesk: " + arg + " not found in map. This should not happen :O", LogLevel::ERR);
        return;
    }

    vdesksMap[vdeskId]->resetLayout();
}

int VirtualDeskManager::prevDeskId(bool backwardCycle) {
    int prevId = activeVdesk()->id - 1;
    if (prevId < 1) {
        prevId = 1;
        if (backwardCycle) {
            auto keys = std::views::keys(vdesksMap);
            prevId    = std::ranges::max(keys);
        }
    }
    return prevId;
}

int VirtualDeskManager::nextDeskId(bool cycle) {
    int nextId = activeVdesk()->id + 1;
    if (cycle) {
        nextId = vdesksMap.contains(nextId) ? nextId : 1;
    }
    return nextId;
}

std::shared_ptr<VirtualDesk> VirtualDeskManager::getOrCreateVdesk(int vdeskId) {
    if (!vdeskNamesMap.contains(vdeskId))
        vdeskNamesMap[vdeskId] = std::to_string(vdeskId);
    if (!vdesksMap.contains(vdeskId)) {
        if (isVerbose())
            printLog("creating new vdesk with id " + std::to_string(vdeskId));
        auto vdesk = vdesksMap[vdeskId] = std::make_shared<VirtualDesk>(vdeskId, vdeskNamesMap[vdeskId]);
        return vdesk;
    }
    return vdesksMap[vdeskId];
}

void VirtualDeskManager::invalidateAllLayouts() {
    for (const auto& [_, vdesk] : vdesksMap) {
        vdesk->invalidateActiveLayout();
    }
}

CMonitor* VirtualDeskManager::getCurrentMonitor() {
    CMonitor* currentMonitor = g_pCompositor->m_pLastMonitor;
    // This can happen when we receive the "on disconnect" signal
    // let's just take first monitor we can find
    if (currentMonitor && (!currentMonitor->m_bEnabled || !currentMonitor->output)) {
        for (std::shared_ptr<CMonitor> mon : g_pCompositor->m_vMonitors) {
            if (mon->m_bEnabled && mon->output)
                return mon.get();
        }
        return nullptr;
    }
    return currentMonitor;
}
