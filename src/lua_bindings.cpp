#include "lua_bindings.hpp"
#include "dispatchers.hpp"
#include "utils.hpp"

#include <plugins/PluginAPI.hpp>

extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
}



template <SDispatchResult (*DispatcherFunc)(std::string)>
int luaDispatcherWrapper(lua_State* L) {
    const std::string arg = luaL_optstring(L, 1, "");
    SDispatchResult result = DispatcherFunc(arg);
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.error.c_str());
        return 2;
    }
    return 1;
}

int luaParseStickyRule(lua_State* L) {
    const std::string value = luaL_checkstring(L, 1);
    parseStickyRule("stickyrule", value.c_str());
    return 0;
}

void registerLuaBindings(HANDLE handle) {
    if (Config::mgr() && Config::mgr()->type() == Config::CONFIG_LUA) {
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", VDESK_DISPATCH_STR, luaDispatcherWrapper<virtualDeskDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", LASTDESK_DISPATCH_STR, luaDispatcherWrapper<goLastVDeskDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", PREVDESK_DISPATCH_STR, luaDispatcherWrapper<goPrevDeskDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", NEXTDESK_DISPATCH_STR, luaDispatcherWrapper<goNextVDeskDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", BACKCYCLE_DISPATCH_STR, luaDispatcherWrapper<cycleBackwardsDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", CYCLEVDESK_DISPATCH_STR, luaDispatcherWrapper<cycleVDeskDispatch>);
        
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", MOVETODESK_DISPATCH_STR, luaDispatcherWrapper<moveToDeskDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", MOVETODESKSILENT_DISPATCH_STR, luaDispatcherWrapper<moveToDeskSilentDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", MOVETOLASTDESK_DISPATCH_STR, luaDispatcherWrapper<moveToLastDeskDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", MOVETOLASTDESKSILENT_DISPATCH_STR, luaDispatcherWrapper<moveToLastDeskSilentDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", MOVETOPREVDESK_DISPATCH_STR, luaDispatcherWrapper<moveToPrevDeskDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", MOVETOPREVDESKSILENT_DISPATCH_STR, luaDispatcherWrapper<moveToPrevDeskSilentDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", MOVETONEXTDESK_DISPATCH_STR, luaDispatcherWrapper<moveToNextDeskDispatch>);
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", MOVETONEXTDESKSILENT_DISPATCH_STR, luaDispatcherWrapper<moveToNextDeskSilentDispatch>);
        
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", RESET_VDESK_DISPATCH_STR, luaDispatcherWrapper<resetVDeskDispatch>);
        
        HyprlandAPI::addLuaFunction(handle, "virtual_desktops", "stickyrule", luaParseStickyRule);
    }
}
