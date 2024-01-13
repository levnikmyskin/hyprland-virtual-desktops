#pragma once

#ifndef VDESK_MANAGER_H
#define VDESK_MANAGER_H

#include "VirtualDesk.hpp"

class VirtualDeskManager {

  public:
    VirtualDeskManager();
    std::unordered_map<int, std::shared_ptr<VirtualDesk>> vdesksMap;
    int                                                   prevDesk      = -1;
    std::unordered_map<int, std::string>                  vdeskNamesMap = {{1, "1"}};
    RememberLayoutConf                                    conf;
    const std::shared_ptr<VirtualDesk>&                   activeVdesk();
    void                                                  changeActiveDesk(std::string&, bool);
    void                                                  changeActiveDesk(int, bool);
    void                                                  previousDesk();
    void                                                  nextDesk(bool cycle);
    void                                                  applyCurrentVDesk();
    int                                                   moveToDesk(std::string&);
    void                                                  loadLayoutConf();
    void                                                  invalidateAllLayouts();
    void                                                  resetAllVdesks();
    void                                                  resetVdesk(const std::string& arg);
    void                                                  deleteInvalidMonitorsOnAllVdesks(const CMonitor*);
    void                                                  deleteInvalidMonitorsOnAllVdesks();

  private:
    int       m_activeDeskKey = 1;
    bool      confLoaded      = false;
    void      cycleWorkspaces();
    int       getDeskIdFromName(const std::string& name, bool createIfNotFound = true);
    CMonitor* getCurrentMonitor();
};
#endif
