#pragma once
#include <plugins/PluginAPI.hpp>
#include <string>

// Dispatchers
SDispatchResult virtualDeskDispatch(std::string arg);
SDispatchResult goLastVDeskDispatch(std::string arg);
SDispatchResult goPrevDeskDispatch(std::string arg);
SDispatchResult goNextVDeskDispatch(std::string arg);
SDispatchResult cycleBackwardsDispatch(std::string arg);
SDispatchResult cycleVDeskDispatch(std::string arg);
SDispatchResult moveToDeskDispatch(std::string arg);
SDispatchResult moveToDeskSilentDispatch(std::string arg);
SDispatchResult moveToLastDeskDispatch(std::string arg);
SDispatchResult moveToLastDeskSilentDispatch(std::string arg);
SDispatchResult moveToPrevDeskDispatch(std::string arg);
SDispatchResult moveToPrevDeskSilentDispatch(std::string arg);
SDispatchResult moveToNextDeskDispatch(std::string arg);
SDispatchResult moveToNextDeskSilentDispatch(std::string arg);
SDispatchResult resetVDeskDispatch(std::string arg);

// Config Keywords
Hyprlang::CParseResult parseStickyRule(const char* command, const char* value);
