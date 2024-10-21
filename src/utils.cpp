#include "utils.hpp"

void printLog(std::string s, LogLevel level) {
    // #ifdef DEBUG
    //     std::cout << "[virtual-desktops] " + s << std::endl;
    // #endif
    Debug::log(level, "[virtual-desktops] {}", s);
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

bool extractBool(std::string& arg) {
    size_t pos;
    bool   cycle = false;
    if ((pos = arg.find(',')) != std::string::npos) {
        cycle = arg.substr(0, pos) == "1";
        arg.erase(0, pos + 1);
    } else {
        cycle = arg == "1";
        arg   = ""; // "consume" string content
    }
    return cycle;
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
    // this might happen if called before plugin is initalized
    if (!PHANDLE)
        return true;
    static auto* const PVERBOSELOGS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, VERBOSE_LOGS)->getDataStaticPtr();
    return **PVERBOSELOGS;
}

std::vector<CSharedPointer<CMonitor>> currentlyEnabledMonitors(const CSharedPointer<CMonitor>& exclude) {
    std::vector<CSharedPointer<CMonitor>> monitors;
    std::copy_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), std::back_inserter(monitors), [&](auto mon) {
        if (g_pCompositor->m_pUnsafeOutput && g_pCompositor->m_pUnsafeOutput->szName == mon->szName)
            return false;

        if (!mon->output)
            return false;

        if (mon->output->name == std::string("HEADLESS-1"))
            return false;

        if (mon == exclude)
            return false;

        return mon->m_bEnabled;
    });
    return monitors;
}

std::string ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(' ');
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(' ');
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string& s) {
    return ltrim(rtrim(s));
}
