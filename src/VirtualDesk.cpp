#include "VirtualDesk.hpp"
#include "globals.hpp"
#include <climits>
#include <algorithm>
#include <src/state/WorkspaceState.hpp>
#include <unordered_set>

VirtualDesk::VirtualDesk(int id, std::string name) {
    this->id   = id;
    this->name = name;
    layouts.push_back(generateCurrentMonitorLayout());
    m_activeLayout_idx = 0;
}

const MonitorLayout& VirtualDesk::activeLayout(const RememberLayoutConf& conf, const CSharedPointer<Monitor::CMonitor>& exclude) {
    if (!activeIsValid) {
        activeIsValid = true;
        searchActiveLayout(conf, exclude);
    }
    return layouts[m_activeLayout_idx];
}

MonitorLayout& VirtualDesk::searchActiveLayout(const RememberLayoutConf& conf, const CSharedPointer<Monitor::CMonitor>& exclude) {

    auto monitors = currentlyEnabledMonitors(exclude);
    switch (conf) {
        case RememberLayoutConf::monitors: {
            // Compute hash set of descriptions
            auto currentSet = setFromMonitors(monitors);
            int  idx        = 0;
            for (auto& layout : layouts) {
                if (layout.empty()) {
                    idx++;
                    continue;
                }
                std::unordered_set<std::string> set;
                for (const auto& [desc, v] : layout) {
                    set.insert(desc);
                }

                bool allPresent = true;
                for (const auto& desc : set) {
                    if (!currentSet.count(desc)) {
                        allPresent = false;
                        break;
                    }
                }
                if (allPresent) {
                    if (isVerbose())
                        printLog("Found layout with monitors");
                    m_activeLayout_idx = idx;
                    return layouts[m_activeLayout_idx];
                }
                idx++;
            }
            break;
        }
        case RememberLayoutConf::size: {
            int idx = 0;
            for (auto& layout : layouts) {
                if (layout.empty()) {
                    idx++;
                    continue;
                }
                if (layout.size() == monitors.size()) {
                    if (isVerbose())
                        printLog("Found layout with size " + std::to_string(layout.size()));

                    // check layout is valid and substitute invalid monitors
                    checkAndAdaptLayout(&layout, exclude);

                    m_activeLayout_idx = idx;
                    return layouts[idx];
                }
                idx++;
            }
            break;
        }
        case RememberLayoutConf::none: layouts.clear();
    }
    // No remembered layout matched. Only mint a new layout while at least one
    // monitor is enabled, so undocking to zero monitors cannot keep appending
    // empty layouts to the list.
    if (!monitors.empty()) {
        layouts.push_back(generateCurrentMonitorLayout());
        m_activeLayout_idx = layouts.size() - 1;
    } else if (layouts.empty()) {
        layouts.push_back(MonitorLayout{});
        m_activeLayout_idx = 0;
    }
    return layouts[m_activeLayout_idx];
}

void VirtualDesk::changeWorkspaceOnMonitor(WORKSPACEID workspaceId, const CSharedPointer<Monitor::CMonitor>& monitor) {
    if (!monitor)
        return;
    layouts[m_activeLayout_idx][monitorDesc(monitor)] = workspaceId;
}

void VirtualDesk::invalidateActiveLayout() {
    activeIsValid = false;
}

void VirtualDesk::resetLayout() {
    layouts[m_activeLayout_idx] = generateCurrentMonitorLayout();
}

void VirtualDesk::deleteInvalidMonitorsOnActiveLayout(const CSharedPointer<Monitor::CMonitor>& exclude) {
    auto          enabled      = currentlyEnabledMonitors(exclude);
    auto          enabledDescs = setFromMonitors(enabled);
    MonitorLayout layout_copy(layouts[m_activeLayout_idx]);
    for (const auto& [desc, workspaceId] : layout_copy) {
        if (enabledDescs.count(desc))
            continue;

        // Re-home the workspace to the least busy enabled monitor, unless that
        // monitor already owns a workspace in this layout.
        auto newMonitor = firstAvailableMonitor(enabled);
        if (newMonitor) {
            auto newDesc = monitorDesc(newMonitor);
            if (!layouts[m_activeLayout_idx].contains(newDesc))
                layouts[m_activeLayout_idx][newDesc] = workspaceId;
        }
        layouts[m_activeLayout_idx].erase(desc);
    }
}

CSharedPointer<Monitor::CMonitor> VirtualDesk::firstAvailableMonitor(const std::vector<CSharedPointer<Monitor::CMonitor>>& enabledMonitors) {
    int                               n = INT_MAX;
    CSharedPointer<Monitor::CMonitor> newMonitor;
    for (const auto& mon : enabledMonitors) {
        auto workspace = State::workspaceState()->query().id(mon->activeWorkspaceID()).run();
        if (workspace) {
            auto n_on_mon = workspace->getWindowCount();
            if (n_on_mon < n) {
                n          = n_on_mon;
                newMonitor = mon;
            }
        }
    }
    return newMonitor;
}

bool VirtualDesk::isWorkspaceOnActiveLayout(WORKSPACEID workspaceId) {
    for (const auto& [_, wid] : layouts[m_activeLayout_idx]) {
        if (workspaceId == wid)
            return true;
    }
    return false;
}

void VirtualDesk::checkAndAdaptLayout(MonitorLayout* layout, const CSharedPointer<Monitor::CMonitor>& exclude) {
    auto enabledMons = currentlyEnabledMonitors(exclude);
    if (enabledMons.empty())
        return;
    auto enabledDescs = setFromMonitors(enabledMons);
    for (const auto& [desc, wid] : MonitorLayout(*layout)) {
        if (!enabledDescs.count(desc)) {
            // Let's try to find a "new" monitor which wasn't in
            // the layout before. If we don't find it, not much we can
            // do except for removing this monitor
            printLog("adapting layout");
            for (const auto& enabledMon : enabledMons) {
                auto enabledDesc = monitorDesc(enabledMon);
                if (!layout->contains(enabledDesc)) {
                    (*layout)[enabledDesc] = wid;
                    (*layout).erase(desc);
                    return;
                }
            }
            (*layout).erase(desc);
        }
    }
}

std::unordered_set<std::string> VirtualDesk::setFromMonitors(const std::vector<CSharedPointer<Monitor::CMonitor>>& monitors) {
    std::unordered_set<std::string> set;
    std::transform(monitors.begin(), monitors.end(), std::inserter(set, set.begin()), [](auto mon) { return monitorDesc(mon); });
    return set;
}

MonitorLayout VirtualDesk::generateCurrentMonitorLayout() {
    MonitorLayout layout;

    auto          monitors = currentlyEnabledMonitors();
    if (PHANDLE && isVerbose())
        printLog("vdesk " + name + " computing new layout for " + std::to_string(monitors.size()) + " monitors");
    auto vdeskFirstWorkspace = (this->id - 1) * monitors.size() + 1;
    int  j                   = 0;
    for (size_t i = vdeskFirstWorkspace; i < vdeskFirstWorkspace + monitors.size(); i++) {
        layout[monitorDesc(monitors[j])] = i;
        j++;
    }
    return layout;
}
