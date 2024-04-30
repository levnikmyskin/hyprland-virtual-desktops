#pragma once

#ifndef STICKYAPPS_H
#define STICKYAPPS_H
#include <string>
#include <VirtualDeskManager.hpp>
#include <hyprland/src/defines.hpp>

const std::string TITLE         = "title";
const std::string INITIAL_TITLE = "initialTitle";
const std::string CLASS         = "class";
const std::string INITIAL_CLASS = "initialClass";

namespace StickyApps {
    struct SStickyRule {
        int         vdesk;
        std::string property;
        std::string value;
    };

    bool              parseRule(const std::string&, SStickyRule&, std::unique_ptr<VirtualDeskManager>&);

    bool              parseWindowRule(const std::string&, SStickyRule&);

    void              matchRules(const std::vector<SStickyRule>&, std::unique_ptr<VirtualDeskManager>&);

    int               matchRuleOnWindow(const std::vector<SStickyRule>&, std::unique_ptr<VirtualDeskManager>&, PHLWINDOW);

    const std::string extractProperty(const SStickyRule&, PHLWINDOW);

    bool              ruleMatch(const std::string&, const std::string&);
}
#endif
