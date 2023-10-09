#include "utils.hpp"

void printLog(std::string s, LogLevel level) {
#ifdef DEBUG
    std::cout << "[virtual-desktops] " + s << std::endl;
#endif
    Debug::log(level, "[virtual-desktops] %s", s);
}

std::string parseMoveDispatch(std::string& arg) {
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

RememberLayoutConf layoutConfFromInt(const int64_t i) {
    switch (i) {
        case 0: return RememberLayoutConf::none;
        case 1: return RememberLayoutConf::size;
        case 2: return RememberLayoutConf::monitors;
        default: return RememberLayoutConf::size;
    }
}

RememberLayoutConf layoutConfFromString(const std::string& conf) {
    if (conf == REMEMBER_NONE)
        return RememberLayoutConf::none;
    else if (conf == REMEMBER_SIZE)
        return RememberLayoutConf::size;
    return RememberLayoutConf::monitors;
}

bool isVerbose() {
    static auto* const PVERBOSELOGS = &HyprlandAPI::getConfigValue(PHANDLE, VERBOSE_LOGS)->intValue;
    return *PVERBOSELOGS;
}
