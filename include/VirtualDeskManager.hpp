
#pragma once

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
    void                                                  applyCurrentVDesk();
    int                                                   moveToDesk(std::string&);
    void                                                  loadLayoutConf();
    void                                                  invalidateAllLayouts();
    void                                                  resetAllVdesks();
    void                                                  deleteInvalidMonitorsOnAllVdesks(CMonitor*);

  private:
    int       m_activeDeskKey = 1;
    bool      confLoaded      = false;
    void      cycleWorkspaces();
    int       getOrCreateDeskIdFromName(const std::string& name);
    CMonitor* getCurrentMonitor();
};
