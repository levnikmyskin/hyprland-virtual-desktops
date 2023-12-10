#include <hyprland/Compositor.hpp>
#include <hyprland/config/ConfigManager.hpp>
#include <hyprland/helpers/Color.hpp>
#include <hyprland/helpers/MiscFunctions.hpp>
#include <hyprland/helpers/Workspace.hpp>
#include <hyprland/debug/Log.hpp>
#include <hyprland/events/Events.hpp>

#include "globals.hpp"
#include "VirtualDeskManager.hpp"
#include "utils.hpp"

#include <any>
#include <iostream>
#include <map>
#include <math.h>
#include <sstream>
#include <vector>
#include <format>

static HOOK_CALLBACK_FN*            onWorkspaceChangeHook   = nullptr;
static HOOK_CALLBACK_FN*            onConfigReloadedHook    = nullptr;
static HOOK_CALLBACK_FN*            onMonitorDisconnectHook = nullptr;
static HOOK_CALLBACK_FN*            onMonitorAddedHook      = nullptr;
static HOOK_CALLBACK_FN*            onRenderHook            = nullptr;
std::unique_ptr<VirtualDeskManager> manager                 = std::make_unique<VirtualDeskManager>();
bool                                notifiedInit            = false;
bool                                needsReloading          = false;
bool                                monitorLayoutChanging   = false;

inline CFunctionHook*               g_pMonitorDestroy = nullptr;
typedef void (*origMonitorDestroy)(void*, void*);

inline CFunctionHook* g_pMonitorAdded = nullptr;
typedef void (*origMonitorAdded)(void*, void*);

void parseNamesConf(std::string& conf) {
    size_t      pos;
    size_t      delim;
    std::string rule;
    try {
        while ((pos = conf.find(',')) != std::string::npos) {
            rule = conf.substr(0, pos);
            if ((delim = rule.find(':')) != std::string::npos) {
                int vdeskId                     = std::stoi(rule.substr(0, delim));
                manager->vdeskNamesMap[vdeskId] = rule.substr(delim + 1);
            }
            conf.erase(0, pos + 1);
        }
        if ((delim = conf.find(':')) != std::string::npos) {
            int vdeskId                     = std::stoi(conf.substr(0, delim));
            manager->vdeskNamesMap[vdeskId] = conf.substr(delim + 1);
        }
        // Update current vdesk names
        for (auto const& [i, vdesk] : manager->vdesksMap) {
            vdesk->name = manager->vdeskNamesMap[i];
        }
    } catch (std::exception const& ex) {
        // #aa1245
        HyprlandAPI::addNotification(PHANDLE, "Syntax error in your virtual-desktops names config", CColor{4289335877}, 8000);
    }
}

void virtualDeskDispatch(std::string arg) {
    manager->changeActiveDesk(arg, true);
}

void goPreviousVDeskDispatch(std::string) {
    manager->previousDesk();
}

void goNextVDeskDispatch(std::string) {
    manager->nextDesk(false);
}

void cycleVDeskDispatch(std::string) {
    manager->nextDesk(true);
}

void moveToDeskDispatch(std::string arg) {
    manager->changeActiveDesk(manager->moveToDesk(arg), true);
}

void moveToDeskSilentDispatch(std::string arg) {
    manager->moveToDesk(arg);
}

void printVdesk(int vdeskId) {
    printLog("VDesk " + std::to_string(vdeskId) + ": " + manager->vdeskNamesMap[vdeskId]);
}

void printVdesk(std::string name) {
    for (auto const& [key, val] : manager->vdeskNamesMap) {
        if (val == name) {
            printLog("Vdesk " + std::to_string(key) + ": " + val);
            return;
        }
    }
}

void printVDeskDispatch(std::string arg) {
    static auto* const PVDESKNAMES = &HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF)->strValue;
    parseNamesConf(*PVDESKNAMES);

    if (arg.length() == 0) {
        printVdesk(manager->activeVdesk()->id);
    } else
        try {
            // maybe id
            printVdesk(std::stoi(arg));
        } catch (std::exception const& ex) {
            // by name then
            printVdesk(arg);
        }
}

void printLayoutDispatch(std::string arg) {
    auto               activeDesk = manager->activeVdesk();
    auto               layout     = activeDesk->activeLayout(manager->conf);
    std::ostringstream out;
    out << "Active desk: " << activeDesk->name;
    out << "\nActive layout size " << layout.size();
    out << "; Monitors:\n";
    for (auto const& [desc, wid] : layout) {
        out << desc << "; Workspace " << wid << "\n";
    }
    printLog(out.str());
}

void resetVDeskDispatch(std::string arg) {
    if (arg.length() == 0) {
        printLog("Resetting all vdesks to default layouts");
        manager->resetAllVdesks();
    } else {
        printLog("Resetting vdesk " + arg);
        manager->resetVdesk(arg);
    }
    manager->applyCurrentVDesk();
}

void onWorkspaceChange(void*, SCallbackInfo&, std::any val) {
    if (monitorLayoutChanging || needsReloading)
        return;
    CWorkspace* workspace   = std::any_cast<CWorkspace*>(val);
    int         workspaceID = std::any_cast<CWorkspace*>(val)->m_iID;

    auto        monitor = g_pCompositor->getMonitorFromID(workspace->m_iMonitorID);
    if (!monitor || !monitor->m_bEnabled)
        return;

    manager->activeVdesk()->changeWorkspaceOnMonitor(workspaceID, monitor);
    if (isVerbose())
        printLog("workspace changed: workspace id " + std::to_string(workspaceID) + "; on monitor " + std::to_string(workspace->m_iMonitorID));
}

void onMonitorDisconnect(void*, SCallbackInfo&, std::any val) {
    CMonitor* monitor = std::any_cast<CMonitor*>(val);
    if (isVerbose())
        printLog("Monitor disconnect called with disabled monitor " + monitor->szName);
    if (currentlyEnabledMonitors(monitor).size() > 0) {
        manager->invalidateAllLayouts();
        manager->deleteInvalidMonitorsOnAllVdesks(monitor);
        needsReloading = true;
    }
}

void onMonitorAdded(void*, SCallbackInfo&, std::any val) {
    CMonitor* monitor = std::any_cast<CMonitor*>(val);
    if (monitor->szName == std::string("HEADLESS-1")) {
        needsReloading = false;
        return;
    }
    manager->invalidateAllLayouts();
    manager->deleteInvalidMonitorsOnAllVdesks();
    manager->applyCurrentVDesk();
    needsReloading = true;
}

void onRender(void*, SCallbackInfo&, std::any val) {
    if (needsReloading) {
        printLog("on render called and needs reloading");
        manager->applyCurrentVDesk();
        needsReloading = false;
    }
}

void onConfigReloaded(void*, SCallbackInfo&, std::any val) {
    static auto* const PNOTIFYINIT = &HyprlandAPI::getConfigValue(PHANDLE, NOTIFY_INIT)->intValue;
    if (*PNOTIFYINIT && !notifiedInit) {
        HyprlandAPI::addNotification(PHANDLE, "Virtual desk Initialized successfully!", CColor{0.f, 1.f, 1.f, 1.f}, 5000);
        notifiedInit = true;
    }
    static auto* const PVDESKNAMES = &HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF)->strValue;
    parseNamesConf(*PVDESKNAMES);
    manager->loadLayoutConf();
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addDispatcher(PHANDLE, VDESK_DISPATCH_STR, virtualDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, PREVDESK_DISPATCH_STR, goPreviousVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, NEXTDESK_DISPATCH_STR, goNextVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, CYCLEVDESK_DISPATCH_STR, cycleVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETODESK_DISPATCH_STR, moveToDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETODESKSILENT_DISPATCH_STR, moveToDeskSilentDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, RESET_VDESK_DISPATCH_STR, resetVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, PRINTDESK_DISPATCH_STR, printVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, PRINTLAYOUT_DISPATCH_STR, printLayoutDispatch);
    HyprlandAPI::addConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF, SConfigValue{.strValue = "unset"});
    HyprlandAPI::addConfigValue(PHANDLE, CYCLEWORKSPACES_CONF, SConfigValue{.intValue = 1});
    HyprlandAPI::addConfigValue(PHANDLE, REMEMBER_LAYOUT_CONF, SConfigValue{.strValue = REMEMBER_SIZE});
    HyprlandAPI::addConfigValue(PHANDLE, NOTIFY_INIT, SConfigValue{.intValue = 1});
    HyprlandAPI::addConfigValue(PHANDLE, VERBOSE_LOGS, SConfigValue{.intValue = 0});

    onWorkspaceChangeHook   = HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", onWorkspaceChange);
    onConfigReloadedHook    = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", onConfigReloaded);
    onMonitorDisconnectHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorRemoved", onMonitorDisconnect);
    onMonitorAddedHook      = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorAdded", onMonitorAdded);
    onRenderHook            = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preRender", onRender);

    // Initialize first vdesk
    HyprlandAPI::reloadConfig();
    return {"virtual-desktops", "Virtual desktop like workspaces", "LevMyskin", "2.0.1"};
}
