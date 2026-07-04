#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>

inline HANDLE PHANDLE = nullptr;

struct SConfig {
    SP<Config::Values::CStringValue> names;
    SP<Config::Values::CIntValue>    cycleWorkspaces;
    SP<Config::Values::CStringValue> rememberLayout;
    SP<Config::Values::CIntValue>    notifyInit;
    SP<Config::Values::CIntValue>    verboseLogging;
};

inline SConfig config = {
    .names           = makeShared<Config::Values::CStringValue>("plugin:virtual-desktops:names", "map a vdesk id with a name", "unset"),
    .cycleWorkspaces = makeShared<Config::Values::CIntValue>("plugin:virtual-desktops:cycleworkspaces", "if set to 1, cycles between vdesks", 1),
    .rememberLayout  = makeShared<Config::Values::CStringValue>("plugin:virtual-desktops:rememberlayout", "chooses how layouts should be remembered", "unset"),
    .notifyInit      = makeShared<Config::Values::CIntValue>("plugin:virtual-desktops:notifyinit", "chooses whether to display the startup notification", 1),
    .verboseLogging  = makeShared<Config::Values::CIntValue>("plugin:virtual-desktops:verbose_logging", "whether to log more stuff", 0),
};
