#pragma once
#ifndef UTILS_H
#define UTILS_H

#include <hyprland/src/debug/Log.hpp>
#include "globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <string>
#include <hyprland/src/Compositor.hpp>

using namespace Hyprutils::Memory;

const std::string VIRTUALDESK_NAMES_CONF = "plugin:virtual-desktops:names";
const std::string CYCLEWORKSPACES_CONF   = "plugin:virtual-desktops:cycleworkspaces";
const std::string REMEMBER_LAYOUT_CONF   = "plugin:virtual-desktops:rememberlayout";
const std::string NOTIFY_INIT            = "plugin:virtual-desktops:notifyinit";
const std::string VERBOSE_LOGS           = "plugin:virtual-desktops:verbose_logging";

const std::string STICKY_RULES_KEYW = "stickyrule";

const std::string VDESK_DISPATCH_STR = "vdesk";

const std::string MOVETODESK_DISPATCH_STR     = "movetodesk";
const std::string MOVETOLASTDESK_DISPATCH_STR = "movetolastdesk";
const std::string MOVETOPREVDESK_DISPATCH_STR = "movetoprevdesk";
const std::string MOVETONEXTDESK_DISPATCH_STR = "movetonextdesk";

const std::string MOVETODESKSILENT_DISPATCH_STR     = "movetodesksilent";
const std::string MOVETOLASTDESKSILENT_DISPATCH_STR = "movetolastdesksilent";
const std::string MOVETOPREVDESKSILENT_DISPATCH_STR = "movetoprevdesksilent";
const std::string MOVETONEXTDESKSILENT_DISPATCH_STR = "movetonextdesksilent";

const std::string LASTDESK_DISPATCH_STR   = "lastdesk";
const std::string PREVDESK_DISPATCH_STR   = "prevdesk";
const std::string NEXTDESK_DISPATCH_STR   = "nextdesk";
const std::string CYCLEVDESK_DISPATCH_STR = "cyclevdesks";
const std::string BACKCYCLE_DISPATCH_STR  = "backcyclevdesks";

const std::string RESET_VDESK_DISPATCH_STR = "vdeskreset";
const std::string PRINTDESK_DISPATCH_STR   = "printdesk";
const std::string PRINTSTATE_DISPATCH_STR  = "printstate";
const std::string PRINTLAYOUT_DISPATCH_STR = "printlayout";

const std::string REMEMBER_NONE     = "none";
const std::string REMEMBER_SIZE     = "size";
const std::string REMEMBER_MONITORS = "monitors";

enum RememberLayoutConf {
    none     = 0,
    size     = 1,
    monitors = 2
};

RememberLayoutConf                    layoutConfFromInt(const int64_t);
RememberLayoutConf                    layoutConfFromString(const std::string& conf);
void                                  printLog(std::string s, LogLevel level = INFO);

std::string                           parseMoveDispatch(std::string& arg);
bool                                  extractBool(std::string& arg);
std::vector<CSharedPointer<CMonitor>> currentlyEnabledMonitors(const CSharedPointer<CMonitor> exclude = nullptr);

std::string                           ltrim(const std::string& s);
std::string                           rtrim(const std::string& s);
std::string                           trim(const std::string& s);

bool                                  isVerbose();
#endif
