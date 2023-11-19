#pragma once

#define VDESK_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <src/helpers/Monitor.hpp>
#include "globals.hpp"
#include "utils.hpp"

typedef std::unordered_map<int, int>         WorkspaceMap;
typedef std::unordered_map<std::string, int> Layout;
typedef std::string                          MonitorName;

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

    const Layout&                    activeLayout(const RememberLayoutConf&);
    Layout&                          searchActiveLayout(const RememberLayoutConf&);
    std::unordered_set<std::string>  setFromMonitors(const std::vector<std::shared_ptr<CMonitor>>&);
    void                             changeWorkspaceOnMonitor(int, CMonitor*);
    void                             invalidateActiveLayout();
    void                             resetLayout();
    void                             deleteInvalidMonitor(const wlr_output*);
    void                             deleteInvalidMonitor();
    void                             deleteInvalidMonitorOnAllLayouts(const wlr_output*);
    static std::shared_ptr<CMonitor> firstAvailableMonitor(const std::vector<std::shared_ptr<CMonitor>>&);

  private:
    int                                           m_activeLayout_idx;
    bool                                          activeIsValid = false;
    Layout                                        generateCurrentMonitorLayout();
    static std::vector<std::shared_ptr<CMonitor>> currentlyEnabledMonitors();
    static std::string                            monitorDesc(const wlr_output*);
    void                                          checkAndAdaptLayout(Layout*);
};
