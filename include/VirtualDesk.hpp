#pragma once

#ifndef VDESK_H
#define VDESK_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <hyprland/helpers/Monitor.hpp>
#include "globals.hpp"
#include "utils.hpp"
#include <hyprland/Compositor.hpp>

typedef std::unordered_map<int, int>             WorkspaceMap;
typedef std::unordered_map<const CMonitor*, int> Layout;
typedef std::string                              MonitorName;

/* 
* Each virtual desk holds a list of layouts. Layouts remember which workspace was on which monitor 
* when those exact monitors (or that exact number of monitors) is/was connected.
* VirtualDeskManager holds instead a map of vdesk_id -> virtual desk.
*/

class VirtualDesk {
  public:
    VirtualDesk(int id = 1, std::string name = "1");
    int                              id;
    std::string                      name;
    std::vector<Layout>              layouts;

    const Layout&                    activeLayout(const RememberLayoutConf&, const CMonitor* exclude = nullptr);
    Layout&                          searchActiveLayout(const RememberLayoutConf&, const CMonitor* exclude = nullptr);
    std::unordered_set<std::string>  setFromMonitors(const std::vector<std::shared_ptr<CMonitor>>&);
    void                             changeWorkspaceOnMonitor(int, CMonitor*);
    void                             invalidateActiveLayout();
    void                             resetLayout();
    CMonitor*                        deleteInvalidMonitor(const CMonitor*);
    void                             deleteInvalidMonitorsOnActiveLayout();
    void                             deleteInvalidMonitorOnAllLayouts(const CMonitor*);
    static std::shared_ptr<CMonitor> firstAvailableMonitor(const std::vector<std::shared_ptr<CMonitor>>&);

  private:
    int                m_activeLayout_idx;
    bool               activeIsValid = false;
    Layout             generateCurrentMonitorLayout();
    static std::string monitorDesc(const CMonitor*);
    void               checkAndAdaptLayout(Layout*, const CMonitor* exclude = nullptr);
};
#endif
