#include "VirtualDeskManager.hpp"
#include <hyprland/src/Compositor.hpp>
#include <format>
#include <algorithm>
#include <hyprland/src/managers/EventManager.hpp>
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
    if (currentlyEnabledMonitors().empty()) {
        printLog("There are no monitors!");
        return;
    }
    if (isVerbose())
        printLog("applying vdesk" + activeVdesk()->name);
    auto         currentMonitor   = getFocusedMonitor();
    auto         layout           = activeVdesk()->activeLayout(conf);
    PHLWORKSPACE focusedWorkspace = nullptr;
    for (const auto& [lmon, workspaceId] : layout) {
        CSharedPointer<CMonitor> mon = lmon;
        if (!mon || !mon->m_enabled) {
            printLog("One of the monitors in the vdesk went bonkers...Will try to find another one");
            mon = activeVdesk()->deleteInvalidMonitor(mon);
            // Big F, we can't do much here
            if (!mon) {
                printLog("There is no enabled monitor at all. I give up :)");
                return;
            }
        }
        PHLWORKSPACE workspace = g_pCompositor->getWorkspaceByID(workspaceId);
        if (!workspace) {
            printLog("Creating workspace " + std::to_string(workspaceId));
            workspace = g_pCompositor->createNewWorkspace(workspaceId, mon->m_id);
        }

        if (workspace->m_monitor != mon)
            g_pCompositor->moveWorkspaceToMonitor(workspace, currentMonitor);

        // Hack: we change the workspace on the current monitor as our last operation,
        // so that we also automatically focus it
        if (currentMonitor && mon->m_id == currentMonitor->m_id) {
            focusedWorkspace = workspace;
            continue;
        }
        mon->changeWorkspace(workspace, false);
    }
    if (currentMonitor && focusedWorkspace)
        currentMonitor->changeWorkspace(focusedWorkspace, false);

    g_pEventManager->postEvent(SHyprIPCEvent{VDESKCHANGE_EVENT_STR, std::to_string(m_activeDeskKey)});
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

    // monitor of the target window
    // if no arg is provided, it's the currently focussed monitor and otherwise
    // it's the monitor of the window matched by the arg regex
    PHLMONITORREF monitor = g_pCompositor->m_lastMonitor;
    if (arg != "") {
        PHLWINDOW window = g_pCompositor->getWindowByRegex(arg);
        if (!window) {
            printLog(std::format("Window {} does not exist???", arg), eLogLevel::ERR);
        } else {
            monitor = window->m_monitor;
        }
    }

    // take the first workspace wherever in the layout
    // and later go for the workspace which is on the same monitor
    // of the window
    auto wid = vdesk->activeLayout(conf).begin()->second;
    for (auto const& [mon, workspace] : vdesk->activeLayout(conf)) {
        if (mon == monitor) {
            wid = workspace;
        }
    }

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
    static auto* const PREMEMBER_LAYOUT = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, REMEMBER_LAYOUT_CONF)->getDataStaticPtr();
    conf                                = layoutConfFromString(*PREMEMBER_LAYOUT);
    confLoaded                          = true;
}

void VirtualDeskManager::cycleWorkspaces() {
    static auto* const PCYCLEWORKSPACES = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, CYCLEWORKSPACES_CONF)->getDataStaticPtr();
    if (!**PCYCLEWORKSPACES)
        return;

    auto                     n_monitors     = g_pCompositor->m_monitors.size();
    CSharedPointer<CMonitor> currentMonitor = g_pCompositor->m_lastMonitor.lock();

    // TODO: implement for more than two monitors as well.
    // This probably requires to compute monitors position
    // in order to consistently move left/right or up/down.
    if (n_monitors == 2) {
        int  other    = g_pCompositor->m_monitors[0]->m_id == currentMonitor->m_id;
        auto otherMon = g_pCompositor->m_monitors[other];
        g_pCompositor->swapActiveWorkspaces(currentMonitor, otherMon);

        auto currentWorkspace = g_pCompositor->getWorkspaceByID(currentMonitor->activeWorkspaceID());
        auto otherWorkspace   = g_pCompositor->getWorkspaceByID(otherMon->activeWorkspaceID());
        activeVdesk()->changeWorkspaceOnMonitor(currentWorkspace->m_id, currentMonitor);
        activeVdesk()->changeWorkspaceOnMonitor(otherWorkspace->m_id, otherMon);
    } else if (n_monitors > 2) {
        printLog("Cycling workspaces is not yet implemented for more than 2 monitors."
                 "\nIf you would like to have this feature, open an issue on virtual-desktops github repo, or even "
                 "better, open a PR :)");
    }
}

void VirtualDeskManager::deleteInvalidMonitorsOnAllVdesks(const CSharedPointer<CMonitor>& monitor) {
    for (const auto& [_, vdesk] : vdesksMap) {
        // recompute active layout
        vdesk->activeLayout(conf, monitor);
        if (monitor)
            printLog(std::format("Deleting monitor with exclude {}", monitor->m_name));
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
        printLog("Reset vdesk: " + arg + " not found", eLogLevel::WARN);
        return;
    }

    if (!vdesksMap.contains(vdeskId)) {
        printLog("Reset vdesk: " + arg + " not found in map. This should not happen :O", eLogLevel::ERR);
        return;
    }

    vdesksMap[vdeskId]->resetLayout();
}

// Returns true if any workspace in the vdesk has at least one window
bool VirtualDeskManager::isDeskPopulated(int vdeskId) {
    if (!vdesksMap.contains(vdeskId))
        return false;
    auto vdesk = vdesksMap[vdeskId];
    for (const auto& [monitor, workspaceId] : vdesk->activeLayout(conf)) {
        auto workspace = g_pCompositor->getWorkspaceByID(workspaceId);
        if (workspace && workspace->getWindows() > 0)
            return true;
    }
    return false;
}

bool VirtualDeskManager::isPopulatedOnlyEnabled() {
    auto* const PCYCLE_POPULATED_ONLY = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, CYCLE_POPULATED_ONLY_CONF)->getDataStaticPtr();
    return **PCYCLE_POPULATED_ONLY;
}

int VirtualDeskManager::prevDeskId(bool backwardCycle) {
    return cycleDeskId(false, backwardCycle);
}

int VirtualDeskManager::nextDeskId(bool cycle) {
    return cycleDeskId(true, cycle);
}

int VirtualDeskManager::cycleDeskId(bool forward, bool allowCycle) {
    bool populatedOnly = isPopulatedOnlyEnabled();
    auto cycle = getCyclingInfo(forward);
    
    int step = forward ? 1 : -1;
    int current = cycle.candidateId;
    
    // Try each desk in sequence
    for (int i = 0; i < (cycle.maxId - cycle.minId + 1); i++) {
        // Forward cycling: allow creation of new desks when cycle=false
        if (forward && !allowCycle && current > cycle.maxId) {
            // Don't create if current desk is empty (avoid double empty desks)
            if (!isDeskPopulated(cycle.currentId)) {
                return cycle.currentId;
            }
            return current;
        }
        
        // Check if current desk is valid
        if (current >= cycle.minId && current <= cycle.maxId && 
            vdesksMap.count(current) && 
            (!populatedOnly || isDeskPopulated(current))) {
            return current;
        }
        
        // Move to next position
        current += step;
        
        // Handle wrap-around
        if (allowCycle) {
            if (current > cycle.maxId) current = cycle.minId;
            else if (current < cycle.minId) current = cycle.maxId;
        } else {
            // No cycling - stop at boundaries
            if (current > cycle.maxId || current < cycle.minId) break;
        }
    }
    
    return cycle.currentId; // No valid desk found
}

inline SCycling VirtualDeskManager::getCyclingInfo(bool forward) {
    int currentId   = activeVdesk()->id;
    int candidateId = forward ? currentId + 1 : currentId - 1;
    
    // Defensive check for empty vdesksMap
    if (vdesksMap.empty()) {
        return {currentId, currentId, currentId, currentId};
    }
    
    auto keys = std::views::keys(vdesksMap);
    int  minId = std::ranges::min(keys);
    int  maxId = std::ranges::max(keys);

    return {currentId, candidateId, minId, maxId};
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

CSharedPointer<CMonitor> VirtualDeskManager::getFocusedMonitor() {
    CWeakPointer<CMonitor> currentMonitor = g_pCompositor->m_lastMonitor;
    // This can happen when we receive the "on disconnect" signal
    // let's just take first monitor we can find
    if (currentMonitor && (!currentMonitor->m_enabled || !currentMonitor->m_output)) {
        for (auto mon : g_pCompositor->m_monitors) {
            if (mon->m_enabled && mon->m_output)
                return mon;
        }
        return nullptr;
    }
    return currentMonitor.lock();
}
