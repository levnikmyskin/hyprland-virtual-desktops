#include <src/Compositor.hpp>
#include <src/config/ConfigManager.hpp>
#include <src/helpers/Color.hpp>
#include <src/helpers/MiscFunctions.hpp>
#include <src/helpers/Workspace.hpp>

#include "globals.hpp"
#include "utils.hpp"

#include <any>
#include <iostream>
#include <map>
#include <math.h>
#include <vector>

const std::string                      VIRTUALDESK_NAMES_CONF        = "plugin:virtual-desktops:names";
const std::string                      CYCLEWORKSPACES_CONF          = "plugin:virtual-desktops:cycleworkspaces";
const std::string                      VDESK_DISPATCH_STR            = "vdesk";
const std::string                      MOVETODESK_DISPATCH_STR       = "movetodesk";
const std::string                      MOVETODESKSILENT_DISPATCH_STR = "movetodesksilent";
const std::string                      PREVDESK_DISPATCH_STR         = "prevdesk";
const std::string                      PRINTDESK_DISPATCH_STR        = "printdesk";

std::map<int, std::string>             virtualDeskNames = {{1, "1"}};
std::vector<std::shared_ptr<CMonitor>> leftRightMonitors;
int                                    prevVDesk    = -1;
int                                    currentVDesk = 1; // when plugin is launched, we assume we start at vdesk 1

static HOOK_CALLBACK_FN*               workspaceChangeHandle = nullptr;
static HOOK_CALLBACK_FN*               monitorAddedHandle    = nullptr;
static HOOK_CALLBACK_FN*               monitorRemovedHandle  = nullptr;

void                                   changeVDesk(int vdesk) {
    if (vdesk == -1) {
        return;
    }
    auto      n_monitors     = g_pCompositor->m_vMonitors.size();
    CMonitor* currentMonitor = g_pCompositor->m_pLastMonitor;
    if (!currentMonitor) {
        printLog("No active monitor!! Not changing vdesk.");
        return;
    }
    printLog("Changing to virtual desktop " + std::to_string(vdesk));
    static auto* const PCYCLEWORKSPACES = &HyprlandAPI::getConfigValue(PHANDLE, CYCLEWORKSPACES_CONF)->intValue;

    if (vdesk == currentVDesk) {
        if (!*PCYCLEWORKSPACES)
            return;
        // TODO implement for more than two monitors as well.
        // initial work is done for at least left-right movement,
        // since we have a list of sorted monitors in leftRightMonitors vector,
        // in order to consistently move left/right or up/down.
        if (n_monitors == 2) {
            int other = g_pCompositor->m_vMonitors[0]->ID == currentMonitor->ID;
            g_pCompositor->swapActiveWorkspaces(currentMonitor, g_pCompositor->m_vMonitors[other].get());
        } else if (n_monitors > 2) {
            printLog("Cycling workspaces is not yet implemented for more than 2 monitors."
                                                                                         "\nIf you would like to have this feature, open an issue on virtual-desktops github repo, or even "
                                                                                         "better, open a PR :)");
        }
        return;
    }
    auto        vdeskFirstWorkspace = (vdesk - 1) * n_monitors + 1;
    int         j                   = 0;

    CWorkspace* focusedWorkspace;
    for (int i = vdeskFirstWorkspace; i < vdeskFirstWorkspace + n_monitors; i++) {
        CWorkspace* workspace = g_pCompositor->getWorkspaceByID(i);
        auto        mon       = g_pCompositor->m_vMonitors[j];
        if (!workspace) {
            printLog("Creating workspace " + std::to_string(i));
            workspace = g_pCompositor->createNewWorkspace(i, mon->ID);
        }
        // Hack: we change the workspace on the current monitor as our last operation,
        // so that we also automatically focus it
        if (mon->ID == currentMonitor->ID) {
            focusedWorkspace = workspace;
            j++;
            continue;
        }
        g_pCompositor->m_vMonitors[j]->changeWorkspace(workspace, false);
        j++;
    }
    currentMonitor->changeWorkspace(focusedWorkspace, false);
}

int getOrCreateDeskIdWithName(const std::string& name) {
    int  max_key = -1;
    bool found   = false;
    int  vdesk;
    for (auto const& [key, val] : virtualDeskNames) {
        if (val == name) {
            vdesk = key;
            found = true;
            break;
        }
        if (key > max_key)
            max_key = key;
    }
    if (!found) {
        vdesk                   = max_key + 1;
        virtualDeskNames[vdesk] = name;
    }
    return vdesk;
}

void virtualDeskDispatch(std::string arg) {
    static auto* const PVDESKNAMES = &HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF)->strValue;
    Utils::parseNamesConf(*PVDESKNAMES, virtualDeskNames);
    int vdesk;
    try {
        vdesk = std::stoi(arg);
    } catch (std::exception const& ex) { vdesk = getOrCreateDeskIdWithName(arg); }
    changeVDesk(vdesk);
}

void goPreviousVDeskDispatch(std::string _) {
    if (prevVDesk == -1) {
        printLog("There's no previous desk");
        return;
    }
    changeVDesk(prevVDesk);
}

int moveToDesk(std::string& arg) {
    // TODO this should be improved:
    //   1. if there's an empty workspace on the specified vdesk, we should move the window there;
    //   2. we should give a way to specify on which workspace to move the window. It'd be best if user could specify 1,2,3
    //      and we move the window to the first, second or third monitor on the vdesk (from left to right)
    if (arg == MOVETODESK_DISPATCH_STR) {
        // TODO notify about missing args
        return -1;
    }

    int  vdeskId;
    auto vdeskName  = Utils::parseMoveDispatch(arg);
    auto n_monitors = g_pCompositor->m_vMonitors.size();
    try {
        vdeskId = std::stoi(vdeskName);
    } catch (std::exception& _) { vdeskId = getOrCreateDeskIdWithName(vdeskName); }

    // just take the first workspace of the vdesk
    auto        wid     = (vdeskId * n_monitors) - (n_monitors - 1);
    std::string moveCmd = std::to_string(wid) + "," + arg;

    HyprlandAPI::invokeHyprctlCommand("dispatch", "movetoworkspacesilent " + moveCmd);
    return vdeskId;
}

void moveToDeskDispatch(std::string arg) {
    changeVDesk(moveToDesk(arg));
}

void moveToDeskSilentDispatch(std::string arg) {
    moveToDesk(arg);
}

void printVdesk(int vdeskId) {
    printLog("VDesk " + std::to_string(vdeskId) + ": " + virtualDeskNames[vdeskId]);
}

void printVdesk(std::string name) {
    for (auto const& [key, val] : virtualDeskNames) {
        if (val == name) {
            printLog("Vdesk " + std::to_string(key) + ": " + val);
            return;
        }
    }
}

void printVDeskDispatch(std::string arg) {
    static auto* const PVDESKNAMES = &HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF)->strValue;
    Utils::parseNamesConf(*PVDESKNAMES, virtualDeskNames);
    if (arg == PRINTDESK_DISPATCH_STR) {
        printVdesk(currentVDesk);
    } else {
        try {
            // maybe id
            printVdesk(std::stoi(arg));
        } catch (std::exception const& ex) {
            // by name then
            printVdesk(arg);
        }
    }
    for (auto m : leftRightMonitors) {
        if (!m->output)
            continue;
        std::cout << "mon id: " << m->ID << ", name: " << m->szName << std::endl;
    }
}

void onWorkspaceChange(void*, std::any val) {
    int  workspaceID = std::any_cast<CWorkspace*>(val)->m_iID;
    auto n_monitors  = g_pCompositor->m_vMonitors.size();
    auto newDesk     = ceil((float)workspaceID / (float)n_monitors);
    if (currentVDesk != newDesk)
        prevVDesk = currentVDesk;
    currentVDesk = newDesk;
}

void refreshMonitorMapping(void*, std::any _) {
    leftRightMonitors = Utils::getMonitorsLeftToRight();
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addDispatcher(PHANDLE, VDESK_DISPATCH_STR, virtualDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, PREVDESK_DISPATCH_STR, goPreviousVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETODESK_DISPATCH_STR, moveToDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETODESKSILENT_DISPATCH_STR, moveToDeskSilentDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, PRINTDESK_DISPATCH_STR, printVDeskDispatch);
    HyprlandAPI::addConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF, SConfigValue{.strValue = "unset"});
    HyprlandAPI::addConfigValue(PHANDLE, CYCLEWORKSPACES_CONF, SConfigValue{.intValue = 1});

    workspaceChangeHandle = HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", onWorkspaceChange);
    monitorAddedHandle    = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorAdded", refreshMonitorMapping);
    monitorRemovedHandle  = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorRemoved", refreshMonitorMapping);

    leftRightMonitors = Utils::getMonitorsLeftToRight();
    HyprlandAPI::reloadConfig();
    HyprlandAPI::addNotification(PHANDLE, "Virtual desk Initialized successfully!", CColor{0.f, 1.f, 1.f, 1.f}, 5000);

    return {"virtual-desktops", "Virtual desktop like workspaces", "LevMyskin", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    virtualDeskNames.clear();
}
