#include "sticky_apps.hpp"
#include "utils.hpp"

bool StickyApps::parseRule(const std::string& rule, SStickyRule& sticky, std::unique_ptr<VirtualDeskManager>& vdeskManager) {
    auto comma_pos = rule.find(',');
    if (comma_pos == std::string::npos)
        return false;
    auto window = rule.substr(0, comma_pos);
    auto vdesk  = rule.substr(comma_pos + 1, rule.length());

    try {
        sticky.vdesk = std::stoi(vdesk);
    } catch (std::exception const& ex) { sticky.vdesk = vdeskManager->getDeskIdFromName(vdesk); }

    return parseWindowRule(window, sticky);
}

bool StickyApps::parseWindowRule(const std::string& rule, SStickyRule& sticky) {
    auto colon_pos = rule.find(':');
    if (colon_pos == std::string::npos)
        return false;
    auto property   = trim(rule.substr(0, colon_pos));
    auto value      = trim(rule.substr(colon_pos + 1, rule.length()));
    sticky.property = property;
    sticky.value    = value;

    return true;
}

void StickyApps::matchRules(const std::vector<SStickyRule>& rules, std::unique_ptr<VirtualDeskManager>& vdeskManager) {
    for (auto& r : rules) {
        for (const auto& w : g_pCompositor->m_vWindows) {
            auto windowProp = extractProperty(r, w);
            if (windowProp.empty())
                continue;
            if (ruleMatch(r.value, windowProp)) {
                printLog(std::format("rule matched {}: {}", r.value, windowProp));
                auto windowPidFmt = std::format("pid:{}", w->getPID());
                auto map          = vdeskManager->vdesksMap;
                if (map.contains(r.vdesk) && map[r.vdesk]->isWorkspaceOnActiveLayout(w->workspaceID()))
                    continue;
                vdeskManager->moveToDesk(windowPidFmt, r.vdesk);
            }
        }
    }
}

int StickyApps::matchRuleOnWindow(const std::vector<SStickyRule>& rules, std::unique_ptr<VirtualDeskManager>& vdeskManager, PHLWINDOW window) {
    for (auto& r : rules) {
        auto windowProp = extractProperty(r, window);
        if (windowProp.empty())
            continue;
        if (ruleMatch(r.value, windowProp)) {
            printLog(std::format("rule matched {}: {}", r.value, windowProp));
            auto windowPidFmt = std::format("pid:{}", window->getPID());
            auto map          = vdeskManager->vdesksMap;
            if (map.find(r.vdesk) != map.end() && map[r.vdesk]->isWorkspaceOnActiveLayout(window->workspaceID()))
                continue;
            vdeskManager->moveToDesk(windowPidFmt, r.vdesk);
            return r.vdesk;
        }
    }
    return -1;
}

const std::string StickyApps::extractProperty(const SStickyRule& rule, PHLWINDOW window) {
    if (rule.property == TITLE) {
        return window->m_szTitle;
    } else if (rule.property == INITIAL_TITLE) {
        return window->m_szInitialTitle;
    } else if (rule.property == CLASS) {
        return window->m_szClass;
    } else if (rule.property == INITIAL_CLASS) {
        return window->m_szInitialClass;
    }
    return "";
}

bool StickyApps::ruleMatch(const std::string& rule, const std::string& str) {
    try {
        std::regex ruleRegex(rule);
        return std::regex_search(str, ruleRegex);
    } catch (...) { printLog(std::format("Regex error for sticky app rule: {}", rule)); }
    return false;
}
