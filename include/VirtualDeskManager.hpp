#pragma once

#ifndef VDESK_MANAGER_H
#define VDESK_MANAGER_H

#include "VirtualDesk.hpp"

class VirtualDeskManager {

  public:
    VirtualDeskManager();
    std::unordered_map<int, std::shared_ptr<VirtualDesk>> vdesksMap;
    std::unordered_map<int, std::vector<PHLWINDOW>>       vdeskWindowsMap;
    int                                                   lastDesk      = -1;
    std::unordered_map<int, std::string>                  vdeskNamesMap = {{1, "1"}};
    RememberLayoutConf                                    conf;
    const std::shared_ptr<VirtualDesk>&                   activeVdesk();
    void                                                  changeActiveDesk(std::string&, bool);
    void                                                  changeActiveDesk(int, bool);
    void                                                  lastVisitedDesk();
    void                                                  prevDesk(bool backwardCycle);
    void                                                  nextDesk(bool cycle);
    void                                                  applyCurrentVDesk();
    int                                                   moveToDesk(std::string&, int vdeskId = -1);
    void                                                  loadLayoutConf();
    void                                                  invalidateAllLayouts();
    void                                                  resetAllVdesks();
    void                                                  resetVdesk(const std::string& arg);
    void                                                  deleteInvalidMonitorsOnAllVdesks(const CMonitor*);
    void                                                  deleteInvalidMonitorsOnAllVdesks();
    int                                                   prevDeskId(bool backwardCycle);
    int                                                   nextDeskId(bool cycle);
    int                                                   getDeskIdFromName(const std::string& name, bool createIfNotFound = true);
    int                                                   getDeskIdForWorkspace(int workspaceId);
    void                                                  moveWindowToDesk(PHLWINDOW window, int vdeskId);
    void                                                  removeWindow(PHLWINDOW window);

  private:
    int                          m_activeDeskKey = 1;
    bool                         confLoaded      = false;
    void                         cycleWorkspaces();
    CMonitor*                    getCurrentMonitor();
    std::shared_ptr<VirtualDesk> getOrCreateVdesk(int vdeskId);
};
#endif
