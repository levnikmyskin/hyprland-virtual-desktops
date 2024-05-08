#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/events/Events.hpp>
#include <hyprland/src/helpers/memory/SharedPtr.hpp>

#include "globals.hpp"
#include "VirtualDeskManager.hpp"
#include "utils.hpp"
#include "sticky_apps.hpp"

#include <any>
#include <vector>

static CSharedPointer<HOOK_CALLBACK_FN> onWorkspaceChangeHook = nullptr;
static CSharedPointer<HOOK_CALLBACK_FN> onWindowOpenHook      = nullptr;
static CSharedPointer<HOOK_CALLBACK_FN> onConfigReloadedHook  = nullptr;

inline CFunctionHook*                   g_pMonitorConnectHook    = nullptr;
inline CFunctionHook*                   g_pMonitorDisconnectHook = nullptr;
typedef void                            (*origMonitorConnect)(void*, bool);
typedef void                            (*origMonitorDisconnect)(void*, bool);

std::unique_ptr<VirtualDeskManager>     manager = std::make_unique<VirtualDeskManager>();
std::vector<StickyApps::SStickyRule>    stickyRules;
bool                                    notifiedInit          = false;
bool                                    monitorLayoutChanging = false;

void                                    parseNamesConf(std::string& conf) {
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

Hyprlang::CParseResult parseStickyRule(const char* command, const char* value) {
    Hyprlang::CParseResult  result;
    StickyApps::SStickyRule rule;
    std::string             value_str = value;
    if (!StickyApps::parseRule(value_str, rule, manager)) {
        std::string err = std::format("Error in your sticky rule: {}", value);
        result.setError(err.c_str());
    } else {
        stickyRules.push_back(rule);
    }
    return result;
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

std::string printVDeskDispatch(eHyprCtlOutputFormat format, std::string arg) {
    static auto* const PVDESKNAMESCONF = (Hyprlang::STRING const*)(HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF))->getDataStaticPtr();

    auto               vdeskNamesConf = std::string{*PVDESKNAMESCONF};
    parseNamesConf(vdeskNamesConf);

    arg.erase(0, PRINTDESK_DISPATCH_STR.length());
    printLog(std::format("Got {}", arg));

    int         vdeskId;
    std::string vdeskName;
    if (arg.length() > 0) {
        arg = arg.erase(0, 1); // delete whitespace
        printLog(std::format("Got {}", arg));
        try {
            // maybe id
            vdeskId   = std::stoi(arg);
            vdeskName = manager->vdeskNamesMap.at(vdeskId);
        } catch (std::exception const& ex) {
            // by name then
            vdeskId = manager->getDeskIdFromName(arg, false);
            if (vdeskId < 0)
                vdeskName = "not found";
            else
                vdeskName = manager->vdeskNamesMap[vdeskId];
        }
    } else {
        vdeskId   = manager->activeVdesk()->id;
        vdeskName = manager->activeVdesk()->name;
    }

    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        return std::format("Virtual desk {}: {}", vdeskId, vdeskName);

    } else if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        return std::format(R"#({{
            "virtualdesk": {{
                "id": {},
                "name": "{}"
            }}
        }})#",
                           vdeskId, vdeskName);
    }
    return "";
}

std::string printLayoutDispatch(eHyprCtlOutputFormat format, std::string arg) {
    auto        activeDesk = manager->activeVdesk();
    auto        layout     = activeDesk->activeLayout(manager->conf);
    std::string out;
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        out += std::format("Active desk: {}\nActive layout size: {};\nMonitors:", activeDesk->name, layout.size());
        for (auto const& [mon, wid] : layout) {
            out += std::format("\n\t{}; Workspace {}", escapeJSONStrings(mon->szName), wid);
        }
    } else if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        out += std::format(R"#({{
            "activeDesk": "{}",
            "activeLayoutSize": {},
            "monitors": [
                )#",
                           activeDesk->name, layout.size());
        for (auto const& [mon, wid] : layout) {
            out += std::format(R"#({{
                "monitorId": {},
                "workspace": {}
            }})#",
                               mon->ID, wid);
        }
        out += "]\n}";
    }
    return out;
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
    if (monitorLayoutChanging)
        return;
    PHLWORKSPACE workspace   = std::any_cast<PHLWORKSPACE>(val);
    int          workspaceID = std::any_cast<PHLWORKSPACE>(val)->m_iID;

    auto         monitor = g_pCompositor->getMonitorFromID(workspace->m_iMonitorID);
    if (!monitor || !monitor->m_bEnabled)
        return;

    manager->activeVdesk()->changeWorkspaceOnMonitor(workspaceID, monitor);
    if (isVerbose())
        printLog("workspace changed: workspace id " + std::to_string(workspaceID) + "; on monitor " + std::to_string(workspace->m_iMonitorID));
}

void onWindowOpen(void*, SCallbackInfo&, std::any val) {
    PHLWINDOW window = std::any_cast<PHLWINDOW>(val);
    int       vdesk  = StickyApps::matchRuleOnWindow(stickyRules, manager, window);
    if (vdesk > 0)
        manager->changeActiveDesk(vdesk, true);
}

void hookMonitorDisconnect(void* thisptr, bool destroy) {
    monitorLayoutChanging = true;
    (*(origMonitorDisconnect)g_pMonitorDisconnectHook->m_pOriginal)(thisptr, destroy);
    monitorLayoutChanging = false;

    CMonitor* monitor = static_cast<CMonitor*>(thisptr);
    if (isVerbose())
        printLog("Monitor disconnect called with disabled monitor " + monitor->szName);
    if (currentlyEnabledMonitors(monitor).size() > 0) {
        manager->invalidateAllLayouts();
        manager->deleteInvalidMonitorsOnAllVdesks(monitor);
        manager->applyCurrentVDesk();
        StickyApps::matchRules(stickyRules, manager);
    }
}

void hookMonitorConnect(void* thisptr, bool noRule) {
    monitorLayoutChanging = true;
    (*(origMonitorConnect)g_pMonitorConnectHook->m_pOriginal)(thisptr, noRule);
    monitorLayoutChanging = false;

    CMonitor* monitor = static_cast<CMonitor*>(thisptr);
    if (monitor->szName == std::string("HEADLESS-1")) {
        return;
    }
    manager->invalidateAllLayouts();
    manager->deleteInvalidMonitorsOnAllVdesks();
    manager->applyCurrentVDesk();
    StickyApps::matchRules(stickyRules, manager);
}

void onConfigReloaded(void*, SCallbackInfo&, std::any val) {
    static auto* const PNOTIFYINIT = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, NOTIFY_INIT)->getDataStaticPtr();
    if (**PNOTIFYINIT && !notifiedInit) {
        HyprlandAPI::addNotification(PHANDLE, "Virtual desk Initialized successfully!", CColor{0.f, 1.f, 1.f, 1.f}, 5000);
        notifiedInit = true;
    }
    static auto* const PVDESKNAMESCONF = (Hyprlang::STRING const*)(HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF))->getDataStaticPtr();
    auto               vdeskNamesConf  = std::string{*PVDESKNAMESCONF};
    parseNamesConf(vdeskNamesConf);
    manager->loadLayoutConf();
}

void registerHyprctlCommands() {
    SHyprCtlCommand cmd;

    // Register printlayout
    cmd.name  = PRINTLAYOUT_DISPATCH_STR;
    cmd.fn    = printLayoutDispatch;
    cmd.exact = true;
    auto ptr  = HyprlandAPI::registerHyprCtlCommand(PHANDLE, cmd);
    if (!ptr)
        printLog(std::format("Failed to register hyprctl command: {}", PRINTLAYOUT_DISPATCH_STR));

    // Register printdesk
    cmd.name  = PRINTDESK_DISPATCH_STR;
    cmd.fn    = printVDeskDispatch;
    cmd.exact = false;
    ptr       = HyprlandAPI::registerHyprCtlCommand(PHANDLE, cmd);
    if (!ptr)
        printLog(std::format("Failed to register hyprctl command: {}", VDESK_DISPATCH_STR));
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

    // Configs
    HyprlandAPI::addConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF, Hyprlang::STRING{"unset"});
    HyprlandAPI::addConfigValue(PHANDLE, CYCLEWORKSPACES_CONF, Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, REMEMBER_LAYOUT_CONF, Hyprlang::STRING{REMEMBER_SIZE.c_str()});
    HyprlandAPI::addConfigValue(PHANDLE, NOTIFY_INIT, Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, VERBOSE_LOGS, Hyprlang::INT{0});

    // Keywords
    HyprlandAPI::addConfigKeyword(PHANDLE, STICKY_RULES_KEYW, parseStickyRule, Hyprlang::SHandlerOptions{});

    onWorkspaceChangeHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", onWorkspaceChange);
    onWindowOpenHook      = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", onWindowOpen);
    onConfigReloadedHook  = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", onConfigReloaded);

    // Function hooks
    static const auto METHODS_CONNECT = HyprlandAPI::findFunctionsByName(PHANDLE, "onConnect");
    g_pMonitorConnectHook             = HyprlandAPI::createFunctionHook(handle, METHODS_CONNECT[0].address, (void*)&hookMonitorConnect);
    g_pMonitorConnectHook->hook();

    static const auto METHODS_DISCONNECT = HyprlandAPI::findFunctionsByName(PHANDLE, "onDisconnect");
    g_pMonitorDisconnectHook             = HyprlandAPI::createFunctionHook(handle, METHODS_DISCONNECT[0].address, (void*)&hookMonitorDisconnect);
    g_pMonitorDisconnectHook->hook();

    registerHyprctlCommands();

    // Initialize first vdesk
    HyprlandAPI::reloadConfig();
    return {"virtual-desktops", "Virtual desktop like workspaces", "LevMyskin", "2.2.2"};
}
