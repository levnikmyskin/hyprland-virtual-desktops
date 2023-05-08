#include "utils.hpp"
#include "globals.hpp"
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

std::vector<int> Utils::getMonitorsLeftToRight() {
    std::vector<int> mons;
    auto             currentMonitor = g_pCompositor->m_pLastMonitor;
    if (!currentMonitor)
        return mons;
    CMonitor* mon;

    for (auto& mon : g_pCompositor->m_vMonitors) {
        if (mon->vecPosition.x == 0) {
            printLog("Found leftmost screen, id " + std::to_string(mon->ID) + " name " + mon->szName);
            mons.push_back(mon->ID);
            g_pCompositor->m_pLastMonitor = mon.get();
            break;
        }
    }

    while ((mon = g_pCompositor->getMonitorInDirection('r'))) {
        mons.push_back(mon->ID);
    }

    g_pCompositor->m_pLastMonitor = currentMonitor;
    for (int i = 0; i < mons.size(); i++) {
        printLog("Screen with id " + std::to_string(mons[i]) + " at position " + std::to_string(i + 1));
    }
    return mons;
}
