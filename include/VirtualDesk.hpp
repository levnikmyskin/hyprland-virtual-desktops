#pragma once

#ifndef VDESK_H
#define VDESK_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "utils.hpp"

using namespace Hyprutils::Memory;

typedef std::unordered_map<int, int> WorkspaceMap;
// Layouts are keyed on the monitor's *description* (falling back to its
// connector name), not on a CSharedPointer<CMonitor>. Keying on a shared
// pointer kept a disconnected monitor alive as a zombie AND lost the remembered
// workspace mapping, because a reconnected monitor is a new object. A plain
// string keeps the memory across a dock/unplug cycle without owning the monitor.
// map with monitor description -> hyprland workspace id
typedef std::unordered_map<std::string, WORKSPACEID> MonitorLayout;
typedef std::string                                  MonitorName;

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
    void                                     deleteInvalidMonitorsOnActiveLayout(const CSharedPointer<Monitor::CMonitor>& exclude = nullptr);
    static CSharedPointer<Monitor::CMonitor> firstAvailableMonitor(const std::vector<CSharedPointer<Monitor::CMonitor>>&);
    bool                                     isWorkspaceOnActiveLayout(WORKSPACEID workspaceId);

  private:
    int                m_activeLayout_idx;
    bool               activeIsValid = false;
    MonitorLayout      generateCurrentMonitorLayout();
    void               checkAndAdaptLayout(MonitorLayout*, const CSharedPointer<Monitor::CMonitor>& exclude = nullptr);
};
#endif
