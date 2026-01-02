#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/desktop/Workspace.hpp>

#include "globals.hpp"
#include "VirtualDeskManager.hpp"
#include "utils.hpp"
#include "sticky_apps.hpp"

#include <any>
#include <vector>

using namespace Hyprutils::Memory;

static CSharedPointer<HOOK_CALLBACK_FN> onWorkspaceChangeHook   = nullptr;
static CSharedPointer<HOOK_CALLBACK_FN> onWindowOpenHook        = nullptr;
static CSharedPointer<HOOK_CALLBACK_FN> onConfigReloadedHook    = nullptr;
static CSharedPointer<HOOK_CALLBACK_FN> onPreMonitorAddedHook   = nullptr;
static CSharedPointer<HOOK_CALLBACK_FN> onMonitorAddedHook      = nullptr;
static CSharedPointer<HOOK_CALLBACK_FN> onPreMonitorRemovedHook = nullptr;
static CSharedPointer<HOOK_CALLBACK_FN> onMonitorRemovedHook    = nullptr;

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
        HyprlandAPI::addNotification(PHANDLE, "Syntax error in your virtual-desktops names config", CHyprColor{4289335877}, 8000);
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

SDispatchResult virtualDeskDispatch(std::string arg) {
    manager->changeActiveDesk(arg, true);
    return SDispatchResult{};
}

SDispatchResult goLastVDeskDispatch(std::string) {
    manager->lastVisitedDesk();
    return SDispatchResult{};
}

SDispatchResult goPrevDeskDispatch(std::string) {
    manager->prevDesk(false);
    return SDispatchResult{};
}

SDispatchResult goNextVDeskDispatch(std::string) {
    manager->nextDesk(false);
    return SDispatchResult{};
}

SDispatchResult cycleBackwardsDispatch(std::string) {
    manager->prevDesk(true);
    return SDispatchResult{};
}

SDispatchResult cycleVDeskDispatch(std::string) {
    manager->nextDesk(true);
    return SDispatchResult{};
}

SDispatchResult moveToDeskDispatch(std::string arg) {
    manager->changeActiveDesk(manager->moveToDesk(arg), true);
    return SDispatchResult{};
}

SDispatchResult moveToDeskSilentDispatch(std::string arg) {
    manager->moveToDesk(arg);
    return SDispatchResult{};
}

SDispatchResult moveToLastDeskDispatch(std::string arg) {
    manager->changeActiveDesk(manager->moveToDesk(arg, manager->lastDesk), true);
    return SDispatchResult{};
}

SDispatchResult moveToLastDeskSilentDispatch(std::string arg) {
    manager->moveToDesk(arg, manager->lastDesk);
    return SDispatchResult{};
}

SDispatchResult moveToPrevDeskDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->changeActiveDesk(manager->moveToDesk(arg, manager->prevDeskId(cycle)), true);
    return SDispatchResult{};
}

SDispatchResult moveToPrevDeskSilentDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->moveToDesk(arg, manager->prevDeskId(cycle));
    return SDispatchResult{};
}

SDispatchResult moveToNextDeskDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->changeActiveDesk(manager->moveToDesk(arg, manager->nextDeskId(cycle)), true);
    return SDispatchResult{};
}

SDispatchResult moveToNextDeskSilentDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->moveToDesk(arg, manager->nextDeskId(cycle));
    return SDispatchResult{};
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

std::string printStateDispatch(eHyprCtlOutputFormat format, std::string arg) {
    std::string out;
    int         entries = 0;

    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        out += "Virtual desks\n";

        for (auto const& [vdeskId, desk] : manager->vdesksMap) {
            unsigned int windows = 0;
            std::string  workspaces;
            bool         first = true;
            for (auto const& [monitor, workspaceId] : desk->activeLayout(manager->conf)) {
                auto workspace = g_pCompositor->getWorkspaceByID(workspaceId);
                if (workspace) {
                    windows += workspace->getWindows();
                }
                if (!first)
                    workspaces += ", ";
                else
                    first = false;
                workspaces += std::format("{}", workspaceId);
            }
            out += std::format("- {}: {}\n  Focused: {}\n  Populated: {}\n  Workspaces: {}\n  Windows: {}\n\n", desk->name, desk->id, manager->activeVdesk().get() == desk.get(),
                               windows > 0, workspaces, windows);
            entries++;
        }
        for (const auto& [vdeskId, name] : manager->vdeskNamesMap) {
            if (manager->vdesksMap.contains(vdeskId))
                continue;
            out += std::format("- {}: {}\n  Focused: false\n  Populated: false\n  Workspaces: \n  Windows: 0\n\n", name, vdeskId);
            entries++;
        }

        // remove last newline
        if (entries > 0)
            out.pop_back();
    } else if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        std::string vdesks;
        for (auto const& [vdeskId, desk] : manager->vdesksMap) {
            unsigned int windows = 0;
            std::string  workspaces;
            bool         first = true;
            for (auto const& [monitor, workspaceId] : desk->activeLayout(manager->conf)) {
                auto workspace = g_pCompositor->getWorkspaceByID(workspaceId);
                if (workspace) {
                    windows += workspace->getWindows();
                }
                if (!first)
                    workspaces += ", ";
                else
                    first = false;
                workspaces += std::format("{}", workspaceId);
            }
            vdesks += std::format(
                R"#({{
    "id": {},
    "name": "{}",
    "focused": {},
    "populated": {},
    "workspaces": [{}],
    "windows": {}
}},)#",
                vdeskId, desk->name, manager->activeVdesk().get() == desk.get(), windows > 0, workspaces, windows);
            entries++;
        }
        for (const auto& [vdeskId, name] : manager->vdeskNamesMap) {
            if (manager->vdesksMap.contains(vdeskId))
                continue;
            vdesks += std::format(
                R"#({{
    "id": {},
    "name": "{}",
    "focused": false,
    "populated": false,
    "workspaces": [],
    "windows": 0
}},)#",
                vdeskId, name);
            entries++;
        }
        // remove last , since this wouldn't be valid json
        vdesks.pop_back();
        out += std::format(R"#([{}])#", vdesks);
    }
    return out;
}

std::string printLayoutDispatch(eHyprCtlOutputFormat format, std::string arg) {
    auto        activeDesk = manager->activeVdesk();
    auto        layout     = activeDesk->activeLayout(manager->conf);
    std::string out;
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        out += std::format("Active desk: {}\nActive layout size: {};\nMonitors:", activeDesk->name, layout.size());
        for (auto const& [mon, wid] : layout) {
            out += std::format("\n\t{}; Workspace {}", escapeJSONStrings(mon->m_name), wid);
        }
    } else if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        out += std::format(R"#({{
            "activeDesk": "{}",
            "activeLayoutSize": {},
            "monitors": [
                )#",
                           activeDesk->name, layout.size());
        int index = 0;
        for (auto const& [mon, wid] : layout) {
            out += std::format(R"#({{
                "monitorId": {},
                "workspace": {}
            }})#",
                               mon->m_id, wid);
            if (++index < layout.size())
                out += ",";
        }
        out += "]\n}";
    }
    return out;
}

SDispatchResult resetVDeskDispatch(std::string arg) {
    if (arg.length() == 0) {
        printLog("Resetting all vdesks to default layouts");
        manager->resetAllVdesks();
    } else {
        printLog("Resetting vdesk " + arg);
        manager->resetVdesk(arg);
    }
    manager->applyCurrentVDesk();
    StickyApps::matchRules(stickyRules, manager);
    return SDispatchResult{};
}

void onWorkspaceChange(void*, SCallbackInfo&, std::any val) {
    if (monitorLayoutChanging)
        return;
    auto        workspace   = std::any_cast<PHLWORKSPACE>(val);
    WORKSPACEID workspaceID = std::any_cast<PHLWORKSPACE>(val)->m_id;

    auto        monitor = workspace->m_monitor.lock();
    if (!monitor || !monitor->m_enabled)
        return;

    manager->activeVdesk()->changeWorkspaceOnMonitor(workspaceID, monitor);
    if (isVerbose()) {
        auto vdesk = manager->activeVdesk();
        printLog("workspace changed on vdesk " + std::to_string(vdesk->id) + ": workspace id " + std::to_string(workspaceID) + "; on monitor " + std::to_string(monitor->m_id));
    }
}

void onWindowOpen(void*, SCallbackInfo&, std::any val) {
    auto window = std::any_cast<PHLWINDOW>(val);
    int  vdesk  = StickyApps::matchRuleOnWindow(stickyRules, manager, window);
    if (vdesk > 0)
        manager->changeActiveDesk(vdesk, true);
}

void onPreMonitorRemoved(void*, SCallbackInfo&, std::any val) {
    CSharedPointer<CMonitor> monitor = std::any_cast<CSharedPointer<CMonitor>>(val);
    if (monitor->m_name == std::string("HEADLESS-1")) {
        return;
    }
    if (isVerbose())
        printLog("Monitor PRE disconnect called with disabled monitor " + monitor->m_name);
    monitorLayoutChanging = true;
}

void onMonitorRemoved(void*, SCallbackInfo&, std::any val) {
    CSharedPointer<CMonitor> monitor = std::any_cast<CSharedPointer<CMonitor>>(val);
    if (monitor->m_name == std::string("HEADLESS-1")) {
        return;
    }
    if (isVerbose())
        printLog("Monitor disconnect called with disabled monitor " + monitor->m_name);
    if (!currentlyEnabledMonitors(monitor).empty()) {
        monitorLayoutChanging = false;
        manager->invalidateAllLayouts();
        manager->deleteInvalidMonitorsOnAllVdesks(monitor);
        manager->applyCurrentVDesk();
        StickyApps::matchRules(stickyRules, manager);
    }
}

void onPreMonitorAdded(void*, SCallbackInfo&, std::any val) {
    CSharedPointer<CMonitor> monitor = std::any_cast<CSharedPointer<CMonitor>>(val);
    if (monitor->m_name == std::string("HEADLESS-1")) {
        return;
    }
    if (isVerbose())
        printLog("Monitor PRE connect called with monitor " + monitor->m_name);
    monitorLayoutChanging = true;
}

void onMonitorAdded(void*, SCallbackInfo&, std::any val) {
    CSharedPointer<CMonitor> monitor = std::any_cast<CSharedPointer<CMonitor>>(val);
    if (monitor->m_name == std::string("HEADLESS-1")) {
        return;
    }
    if (isVerbose())
        printLog("Monitor connect called with monitor " + monitor->m_name);
    monitorLayoutChanging = false;
    manager->invalidateAllLayouts();
    manager->deleteInvalidMonitorsOnAllVdesks();
    manager->applyCurrentVDesk();
    StickyApps::matchRules(stickyRules, manager);
}

void onConfigReloaded(void*, SCallbackInfo&, std::any val) {
    static auto* const PNOTIFYINIT = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, NOTIFY_INIT)->getDataStaticPtr();
    if (**PNOTIFYINIT && !notifiedInit) {
        HyprlandAPI::addNotification(PHANDLE, "Virtual desk Initialized successfully!", CHyprColor{0.f, 1.f, 1.f, 1.f}, 5000);
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

    // Register printstate
    cmd.name  = PRINTSTATE_DISPATCH_STR;
    cmd.fn    = printStateDispatch;
    cmd.exact = true;
    ptr       = HyprlandAPI::registerHyprCtlCommand(PHANDLE, cmd);
    if (!ptr)
        printLog(std::format("Failed to register hyprctl command: {}", PRINTSTATE_DISPATCH_STR));

    // Register printdesk
    cmd.name  = PRINTDESK_DISPATCH_STR;
    cmd.fn    = printVDeskDispatch;
    cmd.exact = false;
    ptr       = HyprlandAPI::registerHyprCtlCommand(PHANDLE, cmd);
    if (!ptr)
        printLog(std::format("Failed to register hyprctl command: {}", PRINTDESK_DISPATCH_STR));
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
    const std::string CLIENT_HASH     = __hyprland_api_get_client_hash();

    // ALWAYS add this to your plugins. It will prevent random crashes coming from
    // mismatched header versions.
    if (COMPOSITOR_HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[virtual-desktops] Mismatched headers! Can't proceed.", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[virtual-desktops] Version mismatch");
    }

    // Dispatchers
    HyprlandAPI::addDispatcherV2(PHANDLE, VDESK_DISPATCH_STR, virtualDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, LASTDESK_DISPATCH_STR, goLastVDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, PREVDESK_DISPATCH_STR, goPrevDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, NEXTDESK_DISPATCH_STR, goNextVDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, BACKCYCLE_DISPATCH_STR, cycleBackwardsDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, CYCLEVDESK_DISPATCH_STR, cycleVDeskDispatch);

    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETODESK_DISPATCH_STR, moveToDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETODESKSILENT_DISPATCH_STR, moveToDeskSilentDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETOLASTDESK_DISPATCH_STR, moveToLastDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETOLASTDESKSILENT_DISPATCH_STR, moveToLastDeskSilentDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETOPREVDESK_DISPATCH_STR, moveToPrevDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETOPREVDESKSILENT_DISPATCH_STR, moveToPrevDeskSilentDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETONEXTDESK_DISPATCH_STR, moveToNextDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETONEXTDESKSILENT_DISPATCH_STR, moveToNextDeskSilentDispatch);

    HyprlandAPI::addDispatcherV2(PHANDLE, RESET_VDESK_DISPATCH_STR, resetVDeskDispatch);

    // Configs
    HyprlandAPI::addConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF, Hyprlang::STRING{"unset"});
    HyprlandAPI::addConfigValue(PHANDLE, CYCLEWORKSPACES_CONF, Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, REMEMBER_LAYOUT_CONF, Hyprlang::STRING{REMEMBER_SIZE.c_str()});
    HyprlandAPI::addConfigValue(PHANDLE, NOTIFY_INIT, Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, VERBOSE_LOGS, Hyprlang::INT{0});

    // Keywords
    HyprlandAPI::addConfigKeyword(PHANDLE, STICKY_RULES_KEYW, parseStickyRule, Hyprlang::SHandlerOptions{});

    onWorkspaceChangeHook   = HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", onWorkspaceChange);
    onWindowOpenHook        = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", onWindowOpen);
    onConfigReloadedHook    = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", onConfigReloaded);
    onPreMonitorAddedHook   = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preMonitorAdded", onPreMonitorAdded);
    onPreMonitorRemovedHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preMonitorRemoved", onPreMonitorRemoved);
    onMonitorAddedHook      = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorAdded", onMonitorAdded);
    onMonitorRemovedHook    = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorRemoved", onMonitorRemoved);

    registerHyprctlCommands();

    // Initialize first vdesk
    HyprlandAPI::reloadConfig();
    return {"virtual-desktops", "Virtual desktop like workspaces", "LevMyskin", "2.2.8"};
}
