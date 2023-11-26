#include "VirtualDesk.hpp"
#include <numeric>
#include <algorithm>
#include <unordered_set>
#include <format>

VirtualDesk::VirtualDesk(int id, std::string name) {
    this->id   = id;
    this->name = name;
    layouts.push_back(generateCurrentMonitorLayout());
    m_activeLayout_idx = 0;
}

const Layout& VirtualDesk::activeLayout(const RememberLayoutConf& conf, const CMonitor* exclude) {
    if (!activeIsValid) {
        activeIsValid = true;
        searchActiveLayout(conf, exclude);
    }
    return layouts[m_activeLayout_idx];
}

Layout& VirtualDesk::searchActiveLayout(const RememberLayoutConf& conf, const CMonitor* exclude) {

    auto monitors = currentlyEnabledMonitors(exclude);
    switch (conf) {
        case RememberLayoutConf::monitors: {
            // Compute hash set of descriptions
            auto currentSet = setFromMonitors(monitors);
            int  idx        = 0;
            for (auto& layout : layouts) {
                std::unordered_set<std::string> set;
                for (const auto& [k, v] : layout) {
                    set.insert(monitorDesc(k));
                }

                std::unordered_set<std::string> intersection;
                std::set_intersection(set.begin(), set.end(), currentSet.begin(), currentSet.end(), std::inserter(intersection, intersection.begin()));
                if (intersection.size() == set.size()) {
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
    layouts.push_back(generateCurrentMonitorLayout());
    m_activeLayout_idx = layouts.size() - 1;
    return layouts[m_activeLayout_idx];
}

void VirtualDesk::changeWorkspaceOnMonitor(int workspaceId, CMonitor* monitor) {
    layouts[m_activeLayout_idx][monitor] = workspaceId;
}

void VirtualDesk::invalidateActiveLayout() {
    activeIsValid = false;
}

void VirtualDesk::resetLayout() {
    layouts[m_activeLayout_idx] = generateCurrentMonitorLayout();
}

void VirtualDesk::deleteInvalidMonitorOnAllLayouts(const CMonitor* monitor) {
    for (auto layout : layouts) {
        deleteInvalidMonitor(monitor);
    }
}

CMonitor* VirtualDesk::deleteInvalidMonitor(const CMonitor* monitor) {
    Layout layout_copy(layouts[m_activeLayout_idx]);
    for (auto const& [mon, workspaceId] : layout_copy) {
        if (mon == monitor) {
            auto newMonitor = firstAvailableMonitor(currentlyEnabledMonitors(monitor));
            if (newMonitor)
                layouts[m_activeLayout_idx][newMonitor.get()] = workspaceId;
            layouts[m_activeLayout_idx].erase(monitor);
            return newMonitor.get();
        }
    }
    return nullptr;
}

void VirtualDesk::deleteInvalidMonitorsOnActiveLayout() {
    Layout                              layout_copy(layouts[m_activeLayout_idx]);
    auto                                enabledMonitors = currentlyEnabledMonitors();
    std::unordered_set<const CMonitor*> enabledMonitors_set;
    for (auto mon : enabledMonitors) {
        enabledMonitors_set.insert(mon.get());
    }
    for (auto [mon, workspaceId] : layout_copy) {
        if (enabledMonitors_set.count(mon) <= 0) {
            auto newMonitor                               = firstAvailableMonitor(enabledMonitors);
            layouts[m_activeLayout_idx][newMonitor.get()] = workspaceId;
            layouts[m_activeLayout_idx].erase(newMonitor.get());
        }
    }
}

std::shared_ptr<CMonitor> VirtualDesk::firstAvailableMonitor(const std::vector<std::shared_ptr<CMonitor>>& enabledMonitors) {
    int                       n = INT_MAX;
    std::shared_ptr<CMonitor> newMonitor;
    for (auto mon : currentlyEnabledMonitors()) {
        auto n_on_mon = g_pCompositor->getWindowsOnWorkspace(mon->activeWorkspace);
        if (n_on_mon < n) {
            n          = n_on_mon;
            newMonitor = mon;
        }
    }
    return newMonitor;
}

void VirtualDesk::checkAndAdaptLayout(Layout* layout, const CMonitor* exclude) {
    auto enabledMons = currentlyEnabledMonitors(exclude);
    if (enabledMons.size() == 0)
        return;
    for (auto [mon, wid] : Layout(*layout)) {
        if (!mon || !mon->m_bEnabled || mon == exclude) {
            // Let's try to find a "new" monitor which wasn't in
            // the layout before. If we don't find it, not much we can
            // do except for removing this monitor
            printLog("adapting layout");
            for (const auto& enabledMon : enabledMons) {
                if (!layout->contains(enabledMon.get())) {
                    (*layout)[enabledMon.get()] = wid;
                    (*layout).erase(mon);
                    return;
                }
            }
            (*layout).erase(mon);
        }
    }
}

std::unordered_set<std::string> VirtualDesk::setFromMonitors(const std::vector<std::shared_ptr<CMonitor>>& monitors) {
    std::unordered_set<std::string> set;
    std::transform(monitors.begin(), monitors.end(), std::inserter(set, set.begin()), [](auto mon) { return monitorDesc(mon.get()); });
    return set;
}

Layout VirtualDesk::generateCurrentMonitorLayout() {
    Layout layout;

    auto   monitors = currentlyEnabledMonitors();
    if (PHANDLE && isVerbose())
        printLog("vdesk " + name + " computing new layout for " + std::to_string(monitors.size()) + " monitors");
    auto vdeskFirstWorkspace = (this->id - 1) * monitors.size() + 1;
    int  j                   = 0;
    for (int i = vdeskFirstWorkspace; i < vdeskFirstWorkspace + monitors.size(); i++) {
        layout[monitors[j].get()] = i;
        j++;
    }
    return layout;
}

std::string VirtualDesk::monitorDesc(const CMonitor* monitor) {
    if (!monitor->output)
        return monitor->szName;
    return monitor->output->description ? monitor->output->description : monitor->szName;
}
