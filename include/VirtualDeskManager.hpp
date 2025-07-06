#pragma once

#ifndef VDESK_MANAGER_H
#define VDESK_MANAGER_H

#include "VirtualDesk.hpp"

struct SCycling {
    int currentId;
    int candidateId;
    int minId;
    int maxId;
};

class VirtualDeskManager {

  public:
    VirtualDeskManager();
    std::unordered_map<int, std::shared_ptr<VirtualDesk>> vdesksMap;
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
    void                                                  deleteInvalidMonitorsOnAllVdesks(const CSharedPointer<CMonitor>&);
    void                                                  deleteInvalidMonitorsOnAllVdesks();
    int                                                   prevDeskId(bool backwardCycle);
    int                                                   nextDeskId(bool cycle);
    int                                                   getDeskIdFromName(const std::string& name, bool createIfNotFound = true);

    // Returns true if any workspace in the vdesk has at least one window
    bool isDeskPopulated(int vdeskId);

  private:
    int                          m_activeDeskKey = 1;
    bool                         confLoaded      = false;
    void                         cycleWorkspaces();
    std::shared_ptr<VirtualDesk> getOrCreateVdesk(int vdeskId);
    CSharedPointer<CMonitor>     getFocusedMonitor();
    inline SCycling              getCyclingInfo(bool forward);
    bool                         isPopulatedOnlyEnabled();
    int                          cycleDeskId(bool forward, bool allowCycle);
};
#endif
