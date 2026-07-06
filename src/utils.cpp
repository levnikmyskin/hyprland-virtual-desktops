#include "utils.hpp"
#include "globals.hpp"
#include <src/state/MonitorState.hpp>
#include <algorithm>
#include <ranges>

void printLog(std::string s, Hyprutils::CLI::eLogLevel level) {
    // #ifdef HYPRLAND_VIRTUAL_DESKTOPS_DEBUG
    //     std::cout << "[virtual-desktops] " + s << std::endl;
    // #endif
    Log::logger->log(level, "[virtual-desktops] {}", s);
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

std::vector<CSharedPointer<Monitor::CMonitor>> currentlyEnabledMonitors(const CSharedPointer<Monitor::CMonitor>& exclude) {
    std::vector<CSharedPointer<Monitor::CMonitor>> monitors;

    if (State::monitorState()->monitors().empty())
        return monitors;

    std::copy_if(State::monitorState()->monitors().begin(), State::monitorState()->monitors().end(), std::back_inserter(monitors), [&](const auto mon) {
        if (!mon)
            return false;

        if (!mon->m_output)
            return false;

        if (mon->m_output->name == std::string("HEADLESS-1"))
            return false;

        if (mon == exclude)
            return false;

        return mon->m_enabled;
    });

    std::string order_str = config.monitorOrder->value();
    if (order_str != "unset" && !order_str.empty()) {
        std::vector<std::string> order;
        for (const auto subrange : std::views::split(order_str, ',')) {
            order.push_back(trim(std::string{subrange.begin(), subrange.end()}));
        }

        std::sort(monitors.begin(), monitors.end(), [&order](const auto& a, const auto& b) {
            auto it_a = std::find(order.begin(), order.end(), a->m_name);
            auto it_b = std::find(order.begin(), order.end(), b->m_name);

            if (it_a != order.end() && it_b != order.end()) {
                return it_a < it_b;
            }
            if (it_a != order.end()) return true;
            if (it_b != order.end()) return false;
            return a->m_name < b->m_name;
        });
    }

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
