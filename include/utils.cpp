#include "utils.hpp"
#include "globals.hpp"

#include <algorithm>
#include <iostream>
#include <src/Compositor.hpp>

void Utils::parseNamesConf(std::string& conf, std::map<int, std::string>& virtualDeskNames) {
    size_t      pos;
    size_t      delim;
    std::string rule;
    try {
        while ((pos = conf.find(',')) != std::string::npos) {
            rule = conf.substr(0, pos);
            if ((delim = rule.find(':')) != std::string::npos) {
                int vdeskId               = std::stoi(rule.substr(0, delim));
                virtualDeskNames[vdeskId] = rule.substr(delim + 1);
            }
            conf.erase(0, pos + 1);
        }
        if ((delim = conf.find(':')) != std::string::npos) {
            int vdeskId               = std::stoi(conf.substr(0, delim));
            virtualDeskNames[vdeskId] = conf.substr(delim + 1);
        }
    } catch (std::exception const& ex) {
        // #aa1245
        HyprlandAPI::addNotification(PHANDLE, "Syntax error in your virtual-desktops names config", CColor{4289335877}, 8000);
    }
}

std::string Utils::parseMoveDispatch(std::string& arg) {
    size_t      pos;
    std::string vdeskName;
    if ((pos = arg.find(',')) != std::string::npos) {
        vdeskName = arg.substr(0, pos);
        arg.erase(0, pos + 1);
    } else {
        vdeskName = arg;
        arg       = "";
    }
    return vdeskName;
}

std::vector<std::shared_ptr<CMonitor>> Utils::getMonitorsLeftToRight() {
    auto monitors = g_pCompositor->m_vMonitors;

    std::sort(monitors.begin(), monitors.end(), [](std::shared_ptr<CMonitor> m1, std::shared_ptr<CMonitor> m2) { return m1->vecPosition.x < m2->vecPosition.x; });

    return monitors;
}
