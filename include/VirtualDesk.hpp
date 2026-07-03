#pragma once

#ifndef VDESK_H
#define VDESK_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "utils.hpp"

using namespace Hyprutils::Memory;

typedef std::unordered_map<int, int> WorkspaceMap;
// map with Monitor::CMonitor* -> hyprland workspace id
typedef std::unordered_map<const CSharedPointer<Monitor::CMonitor>, WORKSPACEID> MonitorLayout;
typedef std::string                                                              MonitorName;

// implement `std::hash` for the CSharedPointer<Monitor> to work with `std::unordered_map`
template <>
struct std::hash<const CSharedPointer<Monitor::CMonitor>> {
    std::size_t operator()(const CSharedPointer<Monitor::CMonitor>& c) const noexcept {
        auto inner = c.get();
        return std::hash<Monitor::CMonitor*>{}(inner);
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
    int                                      id;
    std::string                              name;
    std::vector<MonitorLayout>               layouts;

    const MonitorLayout&                     activeLayout(const RememberLayoutConf&, const CSharedPointer<Monitor::CMonitor>& exclude = nullptr);
    MonitorLayout&                           searchActiveLayout(const RememberLayoutConf&, const CSharedPointer<Monitor::CMonitor>& exclude = nullptr);
    std::unordered_set<std::string>          setFromMonitors(const std::vector<CSharedPointer<Monitor::CMonitor>>&);
    void                                     changeWorkspaceOnMonitor(WORKSPACEID, const CSharedPointer<Monitor::CMonitor>&);
    void                                     invalidateActiveLayout();
    void                                     resetLayout();
    CSharedPointer<Monitor::CMonitor>        deleteInvalidMonitor(const CSharedPointer<Monitor::CMonitor>&);
    void                                     deleteInvalidMonitorsOnActiveLayout();
    void                                     deleteInvalidMonitorOnAllLayouts(const CSharedPointer<Monitor::CMonitor>&);
    static CSharedPointer<Monitor::CMonitor> firstAvailableMonitor(const std::vector<CSharedPointer<Monitor::CMonitor>>&);
    bool                                     isWorkspaceOnActiveLayout(WORKSPACEID workspaceId);

  private:
    int                m_activeLayout_idx;
    bool               activeIsValid = false;
    MonitorLayout      generateCurrentMonitorLayout();
    static std::string monitorDesc(const CSharedPointer<Monitor::CMonitor>&);
    void               checkAndAdaptLayout(MonitorLayout*, const CSharedPointer<Monitor::CMonitor>& exclude = nullptr);
};
#endif
