#include "VirtualDesk.hpp"
#include <src/Compositor.hpp>
#include <numeric>
#include <algorithm>

VirtualDesk::VirtualDesk(int id, std::string name) {
    this->id   = id;
    this->name = name;
    layouts.push_back(generateCurrentMonitorLayout());
    m_activeLayout_idx = 0;
}

const Layout& VirtualDesk::activeLayout(const RememberLayoutConf& conf) {
    if (!activeIsValid) {
        activeIsValid = true;
        searchActiveLayout(conf);
    }
    return layouts[m_activeLayout_idx];
}

Layout& VirtualDesk::searchActiveLayout(const RememberLayoutConf& conf) {

    auto monitors = currentlyEnabledMonitors();
    switch (conf) {
        case RememberLayoutConf::monitors: {
            // Compute hash set of descriptions
            auto currentSet = setFromMonitors(monitors);
            int  idx        = 0;
            for (auto& layout : layouts) {
                std::unordered_set<std::string> set;
                for (const auto& [k, v] : layout) {
                    set.insert(k);
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
        case RememberLayoutConf::layout: {
            int idx = 0;
            for (auto& layout : layouts) {
                if (layout.size() == monitors.size()) {
                    if (isVerbose())
                        printLog("Found layout with size " + std::to_string(layout.size()));
                    // check layout is valid and substitute invalid monitors
                    checkAndAdaptLayout(&layout);

                    m_activeLayout_idx = idx;
                    return layouts[idx];
                }
                idx++;
            }
            break;
        }
        case RememberLayoutConf::none: break;
    }
    layouts.push_back(generateCurrentMonitorLayout());
    m_activeLayout_idx = layouts.size() - 1;
    return layouts[m_activeLayout_idx];
}

void VirtualDesk::changeWorkspaceOnMonitor(int workspaceId, CMonitor* monitor) {
    layouts[m_activeLayout_idx][monitorDesc(*monitor)] = workspaceId;
}

void VirtualDesk::invalidateActiveLayout() {
    activeIsValid = false;
}

void VirtualDesk::resetLayout() {
    layouts[m_activeLayout_idx] = generateCurrentMonitorLayout();
}

void VirtualDesk::deleteInvalidMonitorOnAllLayouts(CMonitor* monitor) {
    for (auto layout : layouts) {
        deleteInvalidMonitor(monitor);
    }
}

void VirtualDesk::deleteInvalidMonitor(CMonitor* monitor) {
    Layout layout_copy(layouts[m_activeLayout_idx]);
    for (auto const& [desc, workspaceId] : layout_copy) {
        if (monitorDesc(*monitor) == desc) {
            int                       n = INT_MAX;
            std::shared_ptr<CMonitor> newMonitor;

            // Just place it on the less busy monitor
            for (auto mon : currentlyEnabledMonitors()) {
                if (mon->ID == monitor->ID)
                    continue;
                auto n_on_mon = g_pCompositor->getWindowsOnWorkspace(mon->activeWorkspace);
                if (n_on_mon < n) {
                    n          = n_on_mon;
                    newMonitor = mon;
                }
            }
            layouts[m_activeLayout_idx][monitorDesc(*newMonitor)] = workspaceId;
            layouts[m_activeLayout_idx].erase(desc);
        }
    }
}

void VirtualDesk::checkAndAdaptLayout(Layout* layout) {
    for (auto [desc, wid] : Layout(*layout)) {
        auto fromDesc = g_pCompositor->getMonitorFromDesc(desc);
        if (!fromDesc || !fromDesc->m_bEnabled) {
            // Let's try to find a "new" monitor which wasn't in
            // the layout before. If we don't find it, not much we can
            // do except for removing this monitor
            for (const auto& mon : currentlyEnabledMonitors()) {
                if (!layout->contains(monitorDesc(*mon))) {
                    (*layout)[monitorDesc(*mon)] = wid;
                    (*layout).erase(desc);
                    return;
                }
            }
            (*layout).erase(desc);
        }
    }
}

std::unordered_set<std::string> VirtualDesk::setFromMonitors(const std::vector<std::shared_ptr<CMonitor>>& monitors) {
    std::unordered_set<std::string> set;
    std::transform(monitors.begin(), monitors.end(), std::inserter(set, set.begin()), [](auto mon) { return monitorDesc(*mon); });
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
        layout[monitorDesc(*monitors[j])] = i;
        j++;
    }
    return layout;
}

std::vector<std::shared_ptr<CMonitor>> VirtualDesk::currentlyEnabledMonitors() {
    std::vector<std::shared_ptr<CMonitor>> monitors;
    std::copy_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), std::back_inserter(monitors), [](auto mon) { return mon->m_bEnabled; });
    return monitors;
}

std::string VirtualDesk::monitorDesc(const CMonitor& monitor) {
    return monitor.output->description ? monitor.output->description : "";
}
