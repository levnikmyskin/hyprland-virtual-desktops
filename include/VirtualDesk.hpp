#pragma once

#ifndef VDESK_H
#define VDESK_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <hyprland/src/helpers/Monitor.hpp>
#include "utils.hpp"
#include <hyprland/src/Compositor.hpp>

using namespace Hyprutils::Memory;

typedef std::unordered_map<int, int> WorkspaceMap;
// map with CMonitor* -> hyprland workspace id
typedef std::unordered_map<const CSharedPointer<CMonitor>, WORKSPACEID> Layout;
typedef std::string                                                     MonitorName;

// implement `std::hash` for the CSharedPointer<Monitor> to work with `std::unordered_map`
template <>
struct std::hash<const CSharedPointer<CMonitor>> {
    std::size_t operator()(const CSharedPointer<CMonitor>& c) const noexcept {
        auto inner = c.get();
        return std::hash<CMonitor*>{}(inner);
    }
};

/*
* Each virtual desk holds a list of layouts. Layouts remember which workspace was on which monitor
* when those exact monitors (or that exact number of monitors) is/was connected.
* VirtualDeskManager holds instead a map of vdesk_id -> virtual desk.
*/

class VirtualDesk {
  public:
    VirtualDesk(int id = 1, std::string name = "1");
    int                             id;
    std::string                     name;
    std::vector<Layout>             layouts;

    const Layout&                   activeLayout(const RememberLayoutConf&, const CSharedPointer<CMonitor>& exclude = nullptr);
    Layout&                         searchActiveLayout(const RememberLayoutConf&, const CSharedPointer<CMonitor>& exclude = nullptr);
    std::unordered_set<std::string> setFromMonitors(const std::vector<CSharedPointer<CMonitor>>&);
    void                            changeWorkspaceOnMonitor(WORKSPACEID, const CSharedPointer<CMonitor>&);
    void                            invalidateActiveLayout();
    void                            resetLayout();
    CSharedPointer<CMonitor>        deleteInvalidMonitor(const CSharedPointer<CMonitor>&);
    void                            deleteInvalidMonitorsOnActiveLayout();
    void                            deleteInvalidMonitorOnAllLayouts(const CSharedPointer<CMonitor>&);
    static CSharedPointer<CMonitor> firstAvailableMonitor(const std::vector<CSharedPointer<CMonitor>>&);
    bool                            isWorkspaceOnActiveLayout(WORKSPACEID workspaceId);

  private:
    int                m_activeLayout_idx;
    bool               activeIsValid = false;
    Layout             generateCurrentMonitorLayout();
    static std::string monitorDesc(const CSharedPointer<CMonitor>&);
    void               checkAndAdaptLayout(Layout*, const CSharedPointer<CMonitor>& exclude = nullptr);
};
#endif
