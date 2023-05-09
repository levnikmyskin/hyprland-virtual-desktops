#pragma once

#include "src/helpers/Monitor.hpp"
#include <string>
#include <map>
#include <vector>
#include <src/debug/Log.hpp>

inline void printLog(const std::string& s) {
    Debug::log(INFO, ("[virtual-desktops] " + s).c_str());
}

namespace Utils {
    void                                   parseNamesConf(std::string&, std::map<int, std::string>&);
    std::string                            parseMoveDispatch(std::string&);
    std::vector<std::shared_ptr<CMonitor>> getMonitorsLeftToRight();
}
