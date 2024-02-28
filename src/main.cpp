#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/helpers/Workspace.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/events/Events.hpp>

#include "globals.hpp"
#include "VirtualDeskManager.hpp"
#include "utils.hpp"
#include "sticky_apps.hpp"

#include <any>
#include <iostream>
#include <sstream>
#include <vector>

static HOOK_CALLBACK_FN*             onWorkspaceChangeHook   = nullptr;
static HOOK_CALLBACK_FN*             onWindowOpenHook        = nullptr;
static HOOK_CALLBACK_FN*             onConfigReloadedHook    = nullptr;
static HOOK_CALLBACK_FN*             onMonitorDisconnectHook = nullptr;
static HOOK_CALLBACK_FN*             onMonitorAddedHook      = nullptr;
static HOOK_CALLBACK_FN*             onRenderHook            = nullptr;
std::unique_ptr<VirtualDeskManager>  manager                 = std::make_unique<VirtualDeskManager>();
std::vector<StickyApps::SStickyRule> stickyRules;
bool                                 notifiedInit          = false;
bool                                 needsReloading        = false;
bool                                 monitorLayoutChanging = false;

inline CFunctionHook*                g_pMonitorDestroy = nullptr;
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

void parseStickyRule(const std::string& command, const std::string& value) {
    StickyApps::SStickyRule rule;
    if (!StickyApps::parseRule(value, rule, manager)) {
        auto err = std::format("Error in your sticky rule: {}", value);
        HyprlandAPI::addNotification(PHANDLE, err, CColor{4289335877}, 8000);
    } else {
        stickyRules.push_back(rule);
    }
}

void virtualDeskDispatch(std::string arg) {
    manager->changeActiveDesk(arg, true);
}

void goLastVDeskDispatch(std::string) {
    manager->lastVisitedDesk();
}

void goPrevDeskDispatch(std::string) {
    manager->prevDesk(false);
}

void goNextVDeskDispatch(std::string) {
    manager->nextDesk(false);
}

void cycleBackwardsDispatch(std::string) {
    manager->prevDesk(true);
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

void moveToLastDeskDispatch(std::string arg) {
    manager->changeActiveDesk(manager->moveToDesk(arg, manager->lastDesk), true);
}

void moveToLastDeskSilentDispatch(std::string arg) {
    manager->moveToDesk(arg, manager->lastDesk);
}

void moveToPrevDeskDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->changeActiveDesk(manager->moveToDesk(arg, manager->prevDeskId(cycle)), true);
}
void moveToPrevDeskSilentDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->moveToDesk(arg, manager->prevDeskId(cycle));
}

void moveToNextDeskDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->changeActiveDesk(manager->moveToDesk(arg, manager->nextDeskId(cycle)), true);
}

void moveToNextDeskSilentDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->moveToDesk(arg, manager->nextDeskId(cycle));
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
    std::string vdesknamesConf = std::any_cast<Hyprlang::STRING>(HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF));

    parseNamesConf(vdesknamesConf);

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
    StickyApps::matchRules(stickyRules, manager);
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

void onWindowOpen(void*, SCallbackInfo&, std::any val) {
    CWindow* window = std::any_cast<CWindow*>(val);
    int      vdesk  = StickyApps::matchRuleOnWindow(stickyRules, manager, window);
    if (vdesk > 0)
        manager->changeActiveDesk(vdesk, true);
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
        StickyApps::matchRules(stickyRules, manager);
        needsReloading = false;
    }
}

void onConfigReloaded(void*, SCallbackInfo&, std::any val) {
    static auto* const PNOTIFYINIT = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, NOTIFY_INIT)->getDataStaticPtr();
    printLog("aiuto");
    if (**PNOTIFYINIT && !notifiedInit) {
        HyprlandAPI::addNotification(PHANDLE, "Virtual desk Initialized successfully!", CColor{0.f, 1.f, 1.f, 1.f}, 5000);
        notifiedInit = true;
    }
    printLog("aiuto2");
    static auto* const PVDESKNAMESCONF = (Hyprlang::STRING const*)(HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF))->getDataStaticPtr();
    auto               vdeskNamesConf  = std::string{*PVDESKNAMESCONF};
    parseNamesConf(vdeskNamesConf);
    manager->loadLayoutConf();
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    // Dispatchers
    HyprlandAPI::addDispatcher(PHANDLE, VDESK_DISPATCH_STR, virtualDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, LASTDESK_DISPATCH_STR, goLastVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, PREVDESK_DISPATCH_STR, goPrevDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, NEXTDESK_DISPATCH_STR, goNextVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, BACKCYCLE_DISPATCH_STR, cycleBackwardsDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, CYCLEVDESK_DISPATCH_STR, cycleVDeskDispatch);

    HyprlandAPI::addDispatcher(PHANDLE, MOVETODESK_DISPATCH_STR, moveToDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETODESKSILENT_DISPATCH_STR, moveToDeskSilentDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETOLASTDESK_DISPATCH_STR, moveToLastDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETOLASTDESKSILENT_DISPATCH_STR, moveToLastDeskSilentDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETOPREVDESK_DISPATCH_STR, moveToPrevDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETOPREVDESKSILENT_DISPATCH_STR, moveToPrevDeskSilentDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETONEXTDESK_DISPATCH_STR, moveToNextDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, MOVETONEXTDESKSILENT_DISPATCH_STR, moveToNextDeskSilentDispatch);

    HyprlandAPI::addDispatcher(PHANDLE, RESET_VDESK_DISPATCH_STR, resetVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, PRINTDESK_DISPATCH_STR, printVDeskDispatch);
    HyprlandAPI::addDispatcher(PHANDLE, PRINTLAYOUT_DISPATCH_STR, printLayoutDispatch);

    // Configs
    HyprlandAPI::addConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF, Hyprlang::STRING{"unset"});
    HyprlandAPI::addConfigValue(PHANDLE, CYCLEWORKSPACES_CONF, Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, REMEMBER_LAYOUT_CONF, Hyprlang::STRING{REMEMBER_SIZE.c_str()});
    HyprlandAPI::addConfigValue(PHANDLE, NOTIFY_INIT, Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, VERBOSE_LOGS, Hyprlang::INT{0});

    // Keywords
    HyprlandAPI::addConfigKeyword(PHANDLE, STICKY_RULES_KEYW, parseStickyRule);

    onWorkspaceChangeHook   = HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", onWorkspaceChange);
    onWindowOpenHook        = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", onWindowOpen);
    onConfigReloadedHook    = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", onConfigReloaded);
    onMonitorDisconnectHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorRemoved", onMonitorDisconnect);
    onMonitorAddedHook      = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorAdded", onMonitorAdded);
    onRenderHook            = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preRender", onRender);

    // Initialize first vdesk
    HyprlandAPI::reloadConfig();
    return {"virtual-desktops", "Virtual desktop like workspaces", "LevMyskin", "2.1.1"};
}
