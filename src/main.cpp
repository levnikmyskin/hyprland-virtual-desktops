#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <cairo/cairo.h>
#include <drm_fourcc.h>

#include "globals.hpp"
#include "VirtualDeskManager.hpp"
#include "utils.hpp"
#include "sticky_apps.hpp"

#include <src/desktop/DesktopTypes.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace Hyprutils::Memory;

static CHyprSignalListener           onWorkspaceChangeHook   = nullptr;
static CHyprSignalListener           onWindowOpenHook        = nullptr;
static CHyprSignalListener           onConfigReloadedHook    = nullptr;
static CHyprSignalListener           onPreMonitorAddedHook   = nullptr;
static CHyprSignalListener           onMonitorAddedHook      = nullptr;
static CHyprSignalListener           onPreMonitorRemovedHook = nullptr;
static CHyprSignalListener           onMonitorRemovedHook    = nullptr;
static CHyprSignalListener           onRenderStageHook       = nullptr;

std::unique_ptr<VirtualDeskManager>  manager = std::make_unique<VirtualDeskManager>();
std::vector<StickyApps::SStickyRule> stickyRules;
bool                                 notifiedInit          = false;
bool                                 monitorLayoutChanging = false;

namespace {
    using Render::GL::g_pHyprOpenGL;
    using Render::ITexture;

    enum class EWallpaperFit {
        COVER,
        CONTAIN,
        STRETCH,
        CENTER,
    };

    struct SWallpaperRule {
        std::string   monitor = "*";
        int           desk    = 0;
        std::string   path;
        EWallpaperFit fit = EWallpaperFit::COVER;
    };

    std::vector<SWallpaperRule>                    g_wallpaperRules;
    std::unordered_map<std::string, SP<ITexture>>  g_wallpaperTextures;
    std::unordered_set<std::string>                g_wallpaperMissingPathsLogged;
    bool                                           g_wallpaperRulesParsedSinceReload = false;

    struct SWallpaperTransition {
        bool                                     active    = false;
        int                                      fromDesk  = 1;
        int                                      toDesk    = 1;
        int                                      direction = 1;
        std::unordered_map<int64_t, WORKSPACEID> fromWorkspaces;
        std::unordered_map<int64_t, WORKSPACEID> toWorkspaces;
        std::unordered_set<int64_t>              settledMonitors;
    };

    SWallpaperTransition g_transition;

    struct SDeskRenderState {
        Vector2D offset;
        bool     found     = false;
        bool     animating = false;
    };

    // Wallpaper drawing lives in its own render-pass element so it can be queued
    // after Hyprland's normal wallpaper pass without touching window rendering.
    class CWallpaperPassElement : public IPassElement {
      public:
        struct SData {
            SP<ITexture> tex;
            CBox         box;
            CBox         clipBox;
            CRegion      damage;
        };

        explicit CWallpaperPassElement(SData&& data) : m_data(std::move(data)) {}

        std::vector<UP<IPassElement>> draw() override {
            const auto oldClip = g_pHyprRenderer->m_renderData.clipBox;
            g_pHyprRenderer->m_renderData.clipBox = m_data.clipBox;

            g_pHyprOpenGL->renderTexture(m_data.tex, m_data.box, {.damage = &m_data.damage, .a = 1.F, .allowDim = false});

            g_pHyprRenderer->m_renderData.clipBox = oldClip;
            return {};
        }

        bool needsLiveBlur() override {
            return false;
        }

        bool needsPrecomputeBlur() override {
            return false;
        }

        const char* passName() override {
            return "CWallpaperPassElement";
        }

        ePassElementType type() override {
            return EK_CUSTOM;
        }

        bool undiscardable() override {
            return true;
        }

        bool disableSimplification() override {
            return true;
        }

        std::optional<CBox> boundingBox() override {
            return m_data.box.copy().scale(1.F / g_pHyprRenderer->m_renderData.pMonitor->m_scale).round();
        }

        CRegion opaqueRegion() override {
            return {};
        }

      private:
        SData m_data;
    };

    bool offsetHasMotion(const Vector2D& vec) {
        return std::abs(vec.x) > 0.5 || std::abs(vec.y) > 0.5;
    }

    std::string lowercase(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) { return std::tolower(ch); });
        return value;
    }

    std::vector<std::string> splitCommaList(const std::string& value) {
        std::vector<std::string> parts;
        size_t                   start = 0;

        while (start <= value.size()) {
            const size_t comma = value.find(',', start);
            if (comma == std::string::npos) {
                parts.push_back(trim(value.substr(start)));
                break;
            }

            parts.push_back(trim(value.substr(start, comma - start)));
            start = comma + 1;
        }

        return parts;
    }

    std::string expandWallpaperPath(std::string path) {
        path = trim(path);
        const auto* home = std::getenv("HOME");
        if (!home)
            return path;

        if (path == "~")
            return home;
        if (path.starts_with("~/"))
            return std::format("{}/{}", home, path.substr(2));
        if (path.starts_with("$HOME/"))
            return std::format("{}/{}", home, path.substr(6));

        return path;
    }

    void replaceAll(std::string& value, const std::string& from, const std::string& to) {
        if (from.empty())
            return;

        size_t pos = 0;
        while ((pos = value.find(from, pos)) != std::string::npos) {
            value.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    std::string resolveWallpaperPath(std::string path, const int deskId, const PHLMONITOR& monitor) {
        replaceAll(path, "{vdesk}", std::to_string(deskId));
        replaceAll(path, "{vdesk0}", std::to_string(std::max(0, deskId - 1)));
        replaceAll(path, "{monitor}", monitor ? monitor->m_name : "");
        return path;
    }

    std::optional<EWallpaperFit> parseWallpaperFit(std::string fit) {
        fit = lowercase(trim(fit));
        if (fit.empty() || fit == "cover" || fit == "fill" || fit == "crop")
            return EWallpaperFit::COVER;
        if (fit == "contain" || fit == "fit")
            return EWallpaperFit::CONTAIN;
        if (fit == "stretch")
            return EWallpaperFit::STRETCH;
        if (fit == "center" || fit == "centre" || fit == "none")
            return EWallpaperFit::CENTER;
        return {};
    }

    std::string wallpaperFitName(const EWallpaperFit fit) {
        switch (fit) {
            case EWallpaperFit::COVER: return "cover";
            case EWallpaperFit::CONTAIN: return "contain";
            case EWallpaperFit::STRETCH: return "stretch";
            case EWallpaperFit::CENTER: return "center";
        }
        return "unknown";
    }

    std::optional<int> parseWallpaperDesk(std::string desk) {
        desk = trim(desk);
        if (desk == "*" || lowercase(desk) == "all")
            return 0;

        try {
            const int id = std::stoi(desk);
            if (id > 0)
                return id;
        } catch (std::exception const&) {}

        return {};
    }

    bool monitorRuleMatches(const std::string& ruleMonitor, const PHLMONITOR& monitor) {
        const auto rule = lowercase(trim(ruleMonitor));
        if (rule.empty() || rule == "*" || rule == "all")
            return true;
        if (!monitor)
            return false;
        return rule == lowercase(monitor->m_name);
    }

    int wallpaperRuleScore(const SWallpaperRule& rule, const int deskId, const PHLMONITOR& monitor) {
        if (rule.desk != 0 && rule.desk != deskId)
            return -1;
        if (!monitorRuleMatches(rule.monitor, monitor))
            return -1;

        int score = 0;
        if (rule.desk == deskId)
            score += 2;
        if (monitor && lowercase(rule.monitor) == lowercase(monitor->m_name))
            score += 4;
        return score;
    }

    const SWallpaperRule* wallpaperRuleForDesk(const int deskId, const PHLMONITOR& monitor) {
        int                   bestScore = -1;
        const SWallpaperRule* best      = nullptr;

        for (const auto& rule : g_wallpaperRules) {
            const int score = wallpaperRuleScore(rule, deskId, monitor);
            if (score < bestScore)
                continue;

            bestScore = score;
            best      = &rule;
        }

        return best;
    }

    void beginWallpaperRuleReload() {
        if (g_wallpaperRulesParsedSinceReload)
            return;

        g_wallpaperRules.clear();
        g_wallpaperTextures.clear();
        g_wallpaperMissingPathsLogged.clear();
        g_wallpaperRulesParsedSinceReload = true;
    }

    void finishWallpaperRuleReload() {
        if (!g_wallpaperRulesParsedSinceReload) {
            g_wallpaperRules.clear();
            g_wallpaperTextures.clear();
            g_wallpaperMissingPathsLogged.clear();
        }

        if (isVerbose())
            printLog(std::format("Loaded {} vdesk wallpaper rules", g_wallpaperRules.size()));

        g_wallpaperRulesParsedSinceReload = false;
    }

    SP<ITexture> wallpaperTextureForPath(const std::string& path) {
        if (auto it = g_wallpaperTextures.find(path); it != g_wallpaperTextures.end())
            return it->second;

        if (path.empty() || !std::filesystem::exists(path)) {
            if (isVerbose() && g_wallpaperMissingPathsLogged.insert(path).second)
                printLog(std::format("Vdesk wallpaper path does not exist: {}", path.empty() ? "<empty>" : path), Log::WARN);
            return nullptr;
        }

        cairo_surface_t* surface = cairo_image_surface_create_from_png(path.c_str());
        const auto       status  = cairo_surface_status(surface);
        if (status != CAIRO_STATUS_SUCCESS) {
            printLog(std::format("Could not load vdesk wallpaper {}: {}", path, cairo_status_to_string(status)), Log::ERR);
            cairo_surface_destroy(surface);
            return nullptr;
        }

        cairo_surface_flush(surface);

        uint32_t drmFormat = DRM_FORMAT_INVALID;
        switch (cairo_image_surface_get_format(surface)) {
            case CAIRO_FORMAT_ARGB32: drmFormat = DRM_FORMAT_ARGB8888; break;
            case CAIRO_FORMAT_RGB24: drmFormat = DRM_FORMAT_XRGB8888; break;
            default:
                printLog(std::format("Unsupported vdesk wallpaper Cairo format for {}", path), Log::ERR);
                cairo_surface_destroy(surface);
                return nullptr;
        }

        // Decode once per desk and hand the pixels to Hyprland as an OpenGL texture.
        auto tex = g_pHyprRenderer->createTexture(drmFormat, cairo_image_surface_get_data(surface), cairo_image_surface_get_stride(surface),
                                                  Vector2D{(double)cairo_image_surface_get_width(surface), (double)cairo_image_surface_get_height(surface)}, true);
        cairo_surface_destroy(surface);

        if (tex && tex->m_texID != 0) {
            g_wallpaperTextures[path] = tex;
            if (isVerbose())
                printLog(std::format("Loaded vdesk wallpaper texture: {}", path));
        }

        return tex;
    }

    WORKSPACEID workspaceIdForMonitorOnDesk(const int deskId, const PHLMONITOR& monitor) {
        if (!monitor)
            return WORKSPACE_INVALID;

        auto deskIt = manager->vdesksMap.find(deskId);
        if (deskIt == manager->vdesksMap.end())
            return WORKSPACE_INVALID;

        for (const auto& [layoutMonitor, workspaceId] : deskIt->second->activeLayout(manager->conf)) {
            if (layoutMonitor && layoutMonitor->m_id == monitor->m_id)
                return workspaceId;
        }

        return WORKSPACE_INVALID;
    }

    void captureWorkspaceIdsForDesk(const int deskId, std::unordered_map<int64_t, WORKSPACEID>& workspaceIds) {
        workspaceIds.clear();

        const auto monitors = currentlyEnabledMonitors();
        for (size_t i = 0; i < monitors.size(); ++i) {
            const auto& monitor = monitors[i];
            auto        workspaceId = workspaceIdForMonitorOnDesk(deskId, monitor);
            // Capture before the vdesk switch. Once layouts change, the old desk's
            // monitor mapping can be stale, so fall back to the plugin's numbering.
            if (workspaceId == WORKSPACE_INVALID)
                workspaceId = (deskId - 1) * monitors.size() + i + 1;

            workspaceIds[static_cast<int64_t>(monitor->m_id)] = workspaceId;
        }
    }

    WORKSPACEID transitionWorkspaceIdForMonitorOnDesk(const int deskId, const PHLMONITOR& monitor) {
        if (!monitor)
            return WORKSPACE_INVALID;

        const auto monitorId = static_cast<int64_t>(monitor->m_id);
        if (deskId == g_transition.fromDesk) {
            if (auto it = g_transition.fromWorkspaces.find(monitorId); it != g_transition.fromWorkspaces.end())
                return it->second;
        } else if (deskId == g_transition.toDesk) {
            if (auto it = g_transition.toWorkspaces.find(monitorId); it != g_transition.toWorkspaces.end())
                return it->second;
        }

        return workspaceIdForMonitorOnDesk(deskId, monitor);
    }

    void beginWallpaperTransition(const int toDesk) {
        if (toDesk < 1 || !manager || !manager->activeVdesk())
            return;

        const int fromDesk = manager->activeVdesk()->id;
        if (fromDesk == toDesk)
            return;

        g_transition.active    = true;
        g_transition.fromDesk  = fromDesk;
        g_transition.toDesk    = toDesk;
        g_transition.direction = toDesk > fromDesk ? 1 : -1;
        captureWorkspaceIdsForDesk(fromDesk, g_transition.fromWorkspaces);
        captureWorkspaceIdsForDesk(toDesk, g_transition.toWorkspaces);
        g_transition.settledMonitors.clear();
        if (isVerbose())
            printLog(std::format("Starting vdesk wallpaper transition {} -> {}", fromDesk, toDesk));
    }

    int targetDeskFromArg(std::string arg) {
        try {
            return std::stoi(arg);
        } catch (std::exception const&) { return manager->getDeskIdFromName(arg); }
    }

    SDeskRenderState renderStateForDeskOnMonitor(const int deskId, const PHLMONITOR& monitor) {
        SDeskRenderState state;

        const auto workspaceId = transitionWorkspaceIdForMonitorOnDesk(deskId, monitor);
        if (workspaceId == WORKSPACE_INVALID)
            return state;

        const auto workspace = g_pCompositor->getWorkspaceByID(workspaceId);
        if (!workspace || !workspace->m_renderOffset)
            return state;

        state.offset    = workspace->m_renderOffset->value();
        state.found     = true;
        state.animating = workspace->m_renderOffset->isBeingAnimated();
        return state;
    }

    void markTransitionMonitorSettled(const PHLMONITOR& monitor) {
        if (!monitor)
            return;

        g_transition.settledMonitors.insert(static_cast<int64_t>(monitor->m_id));
        if (g_transition.settledMonitors.size() >= g_transition.toWorkspaces.size())
            g_transition.active = false;
    }

    CBox wallpaperBoxForMonitor(const SP<ITexture>& tex, const PHLMONITOR& monitor, const EWallpaperFit fit, const Vector2D& offset = {}) {
        const auto monSize  = monitor->m_transformedSize;
        const auto pxOffset = offset * monitor->m_scale;
        CBox       box      = {pxOffset.x, pxOffset.y, monSize.x, monSize.y};

        if (!tex || tex->m_size.x <= 0 || tex->m_size.y <= 0 || monSize.x <= 0 || monSize.y <= 0)
            return box;

        if (fit == EWallpaperFit::STRETCH)
            return box;

        double scale = 1.0;
        if (fit == EWallpaperFit::COVER)
            scale = std::max(monSize.x / tex->m_size.x, monSize.y / tex->m_size.y);
        else if (fit == EWallpaperFit::CONTAIN)
            scale = std::min(monSize.x / tex->m_size.x, monSize.y / tex->m_size.y);

        const double w = tex->m_size.x * scale;
        const double h = tex->m_size.y * scale;

        box.x = pxOffset.x + std::floor((monSize.x - w) / 2.0);
        box.y = pxOffset.y + std::floor((monSize.y - h) / 2.0);
        box.w = std::ceil(w);
        box.h = std::ceil(h);
        return box;
    }

    CRegion fullMonitorRegion(const PHLMONITOR& monitor) {
        const auto monSize = monitor->m_transformedSize;
        return CRegion{0, 0, monSize.x, monSize.y};
    }

    CBox workspaceClipBoxForMonitor(const PHLMONITOR& monitor, const Vector2D& offset = {}) {
        const auto monSize  = monitor->m_transformedSize;
        const auto pxOffset = offset * monitor->m_scale;
        return CBox{pxOffset.x, pxOffset.y, monSize.x, monSize.y};
    }

    void queueWallpaperForDesk(const int deskId, const PHLMONITOR& monitor, const Vector2D& offset) {
        const auto* rule = wallpaperRuleForDesk(deskId, monitor);
        if (!rule)
            return;

        const auto tex = wallpaperTextureForPath(resolveWallpaperPath(rule->path, deskId, monitor));
        if (!tex || !g_pHyprRenderer)
            return;

        CWallpaperPassElement::SData renderData;
        renderData.tex     = tex;
        renderData.box     = wallpaperBoxForMonitor(tex, monitor, rule->fit, offset);
        renderData.damage  = fullMonitorRegion(monitor);
        renderData.clipBox = workspaceClipBoxForMonitor(monitor, offset);

        g_pHyprRenderer->m_renderPass.add(makeUnique<CWallpaperPassElement>(std::move(renderData)));
    }

    void onRenderStage(eRenderStage stage) {
        if (stage != RENDER_POST_WALLPAPER || !g_pHyprOpenGL)
            return;

        static auto* const PWALLPAPERRENDER = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, WALLPAPER_RENDER_CONF)->getDataStaticPtr();
        if (!**PWALLPAPERRENDER)
            return;

        const PHLMONITOR monitor = g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (!monitor || !monitor->m_activeWorkspace)
            return;

        const auto fullDamage = fullMonitorRegion(monitor);
        g_pHyprRenderer->m_renderData.damage.add(fullDamage);
        g_pHyprRenderer->m_renderData.finalDamage.add(fullDamage);

        if (g_transition.active) {
            static auto PWORKSPACEGAP = CConfigValue<Hyprlang::INT>("general:gaps_workspaces");
            const double distance     = monitor->m_size.x + *PWORKSPACEGAP;

            // Follow Hyprland's target workspace offset directly. The outgoing
            // wallpaper is derived from it, so no separate timer or easing exists.
            const auto toState = renderStateForDeskOnMonitor(g_transition.toDesk, monitor);
            if (toState.found && (toState.animating || offsetHasMotion(toState.offset))) {
                const auto toOffset   = toState.offset;
                const auto fromOffset = Vector2D{toOffset.x - g_transition.direction * distance, toOffset.y};
                g_transition.settledMonitors.erase(static_cast<int64_t>(monitor->m_id));
                queueWallpaperForDesk(g_transition.fromDesk, monitor, fromOffset);
                queueWallpaperForDesk(g_transition.toDesk, monitor, toOffset);
                g_pHyprRenderer->damageMonitor(monitor);
                return;
            }

            const auto fromState = renderStateForDeskOnMonitor(g_transition.fromDesk, monitor);
            if (!toState.found && fromState.found && fromState.animating) {
                const auto fromOffset = fromState.offset;
                const auto toOffset   = Vector2D{fromOffset.x + g_transition.direction * distance, fromOffset.y};
                g_transition.settledMonitors.erase(static_cast<int64_t>(monitor->m_id));
                queueWallpaperForDesk(g_transition.fromDesk, monitor, fromOffset);
                queueWallpaperForDesk(g_transition.toDesk, monitor, toOffset);
                g_pHyprRenderer->damageMonitor(monitor);
                return;
            }

            markTransitionMonitorSettled(monitor);
        }

        queueWallpaperForDesk(manager->activeVdesk()->id, monitor, {});
    }
}

void                                 parseNamesConf(std::string& conf) {
    size_t      pos;
    size_t      delim;
    std::string rule;
    try {
        while ((pos = conf.find(',')) != std::string::npos) {
            rule = conf.substr(0, pos);
            if ((delim = rule.find(':')) != std::string::npos) {
                int vdeskId                     = std::stoi(rule.substr(0, delim));
                manager->vdeskNamesMap[vdeskId] = rule.substr(delim + 1);
            }
            conf.erase(0, pos + 1);
        }
        if ((delim = conf.find(':')) != std::string::npos) {
            int vdeskId                     = std::stoi(conf.substr(0, delim));
            manager->vdeskNamesMap[vdeskId] = conf.substr(delim + 1);
        }
        // Update current vdesk names
        for (auto const& [i, vdesk] : manager->vdesksMap) {
            vdesk->name = manager->vdeskNamesMap[i];
        }
    } catch (std::exception const& ex) {
        // #aa1245
        HyprlandAPI::addNotification(PHANDLE, "Syntax error in your virtual-desktops names config", CHyprColor{4289335877}, 8000);
    }
}

Hyprlang::CParseResult parseStickyRule(const char* command, const char* value) {
    Hyprlang::CParseResult  result;
    StickyApps::SStickyRule rule;
    std::string             value_str = value;
    if (!StickyApps::parseRule(value_str, rule, manager)) {
        std::string err = std::format("Error in your sticky rule: {}", value);
        result.setError(err.c_str());
    } else {
        stickyRules.push_back(rule);
    }
    return result;
}

Hyprlang::CParseResult parseWallpaperRule(const char* command, const char* value) {
    Hyprlang::CParseResult result;
    const auto             parts = splitCommaList(value);

    if (parts.size() < 3 || parts.size() > 4) {
        result.setError("vdeskwallpaper syntax: monitor, vdesk, path, mode");
        return result;
    }

    const auto desk = parseWallpaperDesk(parts[1]);
    if (!desk) {
        result.setError("vdeskwallpaper vdesk must be a positive number or *");
        return result;
    }

    const auto fit = parseWallpaperFit(parts.size() >= 4 ? parts[3] : "cover");
    if (!fit) {
        result.setError("vdeskwallpaper mode must be cover, contain, fit, stretch, or center");
        return result;
    }

    const auto path = expandWallpaperPath(parts[2]);
    if (path.empty()) {
        result.setError("vdeskwallpaper path cannot be empty");
        return result;
    }

    beginWallpaperRuleReload();
    const SWallpaperRule rule{
        .monitor = parts[0].empty() ? "*" : parts[0],
        .desk    = *desk,
        .path    = path,
        .fit     = *fit,
    };
    g_wallpaperRules.push_back(rule);

    if (isVerbose()) {
        const auto deskLabel = rule.desk == 0 ? std::string{"*"} : std::to_string(rule.desk);
        printLog(std::format("Registered vdesk wallpaper rule: monitor={}, desk={}, path={}, mode={}", rule.monitor, deskLabel, rule.path, wallpaperFitName(rule.fit)));
    }

    return result;
}

SDispatchResult virtualDeskDispatch(std::string arg) {
    beginWallpaperTransition(targetDeskFromArg(arg));
    manager->changeActiveDesk(arg, true);
    return SDispatchResult{};
}

SDispatchResult goLastVDeskDispatch(std::string) {
    if (manager->lastDesk > 0) {
        beginWallpaperTransition(manager->lastDesk);
        manager->changeActiveDesk(manager->lastDesk, true);
    } else {
        manager->lastVisitedDesk();
    }
    return SDispatchResult{};
}

SDispatchResult goPrevDeskDispatch(std::string) {
    const int targetDesk = manager->prevDeskId(false);
    beginWallpaperTransition(targetDesk);
    manager->changeActiveDesk(targetDesk, true);
    return SDispatchResult{};
}

SDispatchResult goNextVDeskDispatch(std::string) {
    const int targetDesk = manager->nextDeskId(false);
    beginWallpaperTransition(targetDesk);
    manager->changeActiveDesk(targetDesk, true);
    return SDispatchResult{};
}

SDispatchResult cycleBackwardsDispatch(std::string) {
    const int targetDesk = manager->prevDeskId(true);
    beginWallpaperTransition(targetDesk);
    manager->changeActiveDesk(targetDesk, true);
    return SDispatchResult{};
}

SDispatchResult cycleVDeskDispatch(std::string) {
    const int targetDesk = manager->nextDeskId(true);
    beginWallpaperTransition(targetDesk);
    manager->changeActiveDesk(targetDesk, true);
    return SDispatchResult{};
}

SDispatchResult moveToDeskDispatch(std::string arg) {
    const int targetDesk = manager->moveToDesk(arg);
    beginWallpaperTransition(targetDesk);
    manager->changeActiveDesk(targetDesk, true);
    return SDispatchResult{};
}

SDispatchResult moveToDeskSilentDispatch(std::string arg) {
    manager->moveToDesk(arg);
    return SDispatchResult{};
}

SDispatchResult moveToLastDeskDispatch(std::string arg) {
    const int targetDesk = manager->moveToDesk(arg, manager->lastDesk);
    beginWallpaperTransition(targetDesk);
    manager->changeActiveDesk(targetDesk, true);
    return SDispatchResult{};
}

SDispatchResult moveToLastDeskSilentDispatch(std::string arg) {
    manager->moveToDesk(arg, manager->lastDesk);
    return SDispatchResult{};
}

SDispatchResult moveToPrevDeskDispatch(std::string arg) {
    bool      cycle      = extractBool(arg);
    const int targetDesk = manager->moveToDesk(arg, manager->prevDeskId(cycle));
    beginWallpaperTransition(targetDesk);
    manager->changeActiveDesk(targetDesk, true);
    return SDispatchResult{};
}

SDispatchResult moveToPrevDeskSilentDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->moveToDesk(arg, manager->prevDeskId(cycle));
    return SDispatchResult{};
}

SDispatchResult moveToNextDeskDispatch(std::string arg) {
    bool      cycle      = extractBool(arg);
    const int targetDesk = manager->moveToDesk(arg, manager->nextDeskId(cycle));
    beginWallpaperTransition(targetDesk);
    manager->changeActiveDesk(targetDesk, true);
    return SDispatchResult{};
}

SDispatchResult moveToNextDeskSilentDispatch(std::string arg) {
    bool cycle = extractBool(arg);
    manager->moveToDesk(arg, manager->nextDeskId(cycle));
    return SDispatchResult{};
}

std::string printVDeskDispatch(eHyprCtlOutputFormat format, std::string arg) {
    static auto* const PVDESKNAMESCONF = (Hyprlang::STRING const*)(HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF))->getDataStaticPtr();

    auto               vdeskNamesConf = std::string{*PVDESKNAMESCONF};
    parseNamesConf(vdeskNamesConf);

    arg.erase(0, PRINTDESK_DISPATCH_STR.length());

    int         vdeskId;
    std::string vdeskName;
    if (arg.length() > 0) {
        arg = arg.erase(0, 1); // delete whitespace
        try {
            // maybe id
            vdeskId   = std::stoi(arg);
            vdeskName = manager->vdeskNamesMap.at(vdeskId);
        } catch (std::exception const& ex) {
            // by name then
            vdeskId = manager->getDeskIdFromName(arg, false);
            if (vdeskId < 0)
                vdeskName = "not found";
            else
                vdeskName = manager->vdeskNamesMap[vdeskId];
        }
    } else {
        vdeskId   = manager->activeVdesk()->id;
        vdeskName = manager->activeVdesk()->name;
    }

    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        return std::format("Virtual desk {}: {}", vdeskId, vdeskName);

    } else if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        return std::format(R"#({{
    "virtualdesk": {{
        "id": {},
        "name": "{}"
    }}
}})#",
                           vdeskId, vdeskName);
    }
    return "";
}

std::string printStateDispatch(eHyprCtlOutputFormat format, std::string arg) {
    std::string out;
    int         entries = 0;

    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        out += "Virtual desks\n";

        for (auto const& [vdeskId, desk] : manager->vdesksMap) {
            unsigned int windows = 0;
            std::string  workspaces;
            bool         first = true;
            for (auto const& [monitor, workspaceId] : desk->activeLayout(manager->conf)) {
                auto workspace = g_pCompositor->getWorkspaceByID(workspaceId);
                if (workspace) {
                    windows += workspace->getWindows();
                }
                if (!first)
                    workspaces += ", ";
                else
                    first = false;
                workspaces += std::format("{}", workspaceId);
            }
            out += std::format("- {}: {}\n  Focused: {}\n  Populated: {}\n  Workspaces: {}\n  Windows: {}\n\n", desk->name, desk->id, manager->activeVdesk().get() == desk.get(),
                               windows > 0, workspaces, windows);
            entries++;
        }
        for (const auto& [vdeskId, name] : manager->vdeskNamesMap) {
            if (manager->vdesksMap.contains(vdeskId))
                continue;
            out += std::format("- {}: {}\n  Focused: false\n  Populated: false\n  Workspaces: \n  Windows: 0\n\n", name, vdeskId);
            entries++;
        }

        // remove last newline
        if (entries > 0)
            out.pop_back();
    } else if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        std::string vdesks;
        for (auto const& [vdeskId, desk] : manager->vdesksMap) {
            unsigned int windows = 0;
            std::string  workspaces;
            bool         first = true;
            for (auto const& [monitor, workspaceId] : desk->activeLayout(manager->conf)) {
                auto workspace = g_pCompositor->getWorkspaceByID(workspaceId);
                if (workspace) {
                    windows += workspace->getWindows();
                }
                if (!first)
                    workspaces += ", ";
                else
                    first = false;
                workspaces += std::format("{}", workspaceId);
            }
            vdesks += std::format(
                R"#({{
    "id": {},
    "name": "{}",
    "focused": {},
    "populated": {},
    "workspaces": [{}],
    "windows": {}
}},)#",
                vdeskId, desk->name, manager->activeVdesk().get() == desk.get(), windows > 0, workspaces, windows);
            entries++;
        }
        for (const auto& [vdeskId, name] : manager->vdeskNamesMap) {
            if (manager->vdesksMap.contains(vdeskId))
                continue;
            vdesks += std::format(
                R"#({{
    "id": {},
    "name": "{}",
    "focused": false,
    "populated": false,
    "workspaces": [],
    "windows": 0
}},)#",
                vdeskId, name);
            entries++;
        }
        // remove last , since this wouldn't be valid json
        vdesks.pop_back();
        out += std::format(R"#([{}])#", vdesks);
    }
    return out;
}

std::string printLayoutDispatch(eHyprCtlOutputFormat format, std::string arg) {
    auto        activeDesk = manager->activeVdesk();
    auto        layout     = activeDesk->activeLayout(manager->conf);
    std::string out;
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        out += std::format("Active desk: {}\nActive layout size: {};\nMonitors:", activeDesk->name, layout.size());
        for (auto const& [mon, wid] : layout) {
            out += std::format("\n\t{}; Workspace {}", escapeJSONStrings(mon->m_name), wid);
        }
    } else if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        out += std::format(R"#({{
            "activeDesk": "{}",
            "activeLayoutSize": {},
            "monitors": [
                )#",
                           activeDesk->name, layout.size());
        size_t index = 0;
        for (auto const& [mon, wid] : layout) {
            out += std::format(R"#({{
                "monitorId": {},
                "workspace": {}
            }})#",
                               mon->m_id, wid);
            if (++index < layout.size())
                out += ",";
        }
        out += "]\n}";
    }
    return out;
}

SDispatchResult resetVDeskDispatch(std::string arg) {
    if (arg.length() == 0) {
        printLog("Resetting all vdesks to default layouts");
        manager->resetAllVdesks();
    } else {
        printLog("Resetting vdesk " + arg);
        manager->resetVdesk(arg);
    }
    manager->applyCurrentVDesk();
    StickyApps::matchRules(stickyRules, manager);
    return SDispatchResult{};
}

void onWorkspaceChange(PHLWORKSPACE workspace) {
    if (monitorLayoutChanging)
        return;

    auto monitor = workspace->m_monitor.lock();
    if (!monitor || !monitor->m_enabled)
        return;

    manager->activeVdesk()->changeWorkspaceOnMonitor(workspace->m_id, monitor);
    if (isVerbose()) {
        auto vdesk = manager->activeVdesk();
        printLog("workspace changed on vdesk " + std::to_string(vdesk->id) + ": workspace id " + std::to_string(workspace->m_id) + "; on monitor " + std::to_string(monitor->m_id));
    }
}

void onWindowOpen(PHLWINDOW window) {
    // auto window = std::any_cast<PHLWINDOW>(val);
    int vdesk = StickyApps::matchRuleOnWindow(stickyRules, manager, window);
    if (vdesk > 0)
        manager->changeActiveDesk(vdesk, true);
}

void onPreMonitorRemoved(PHLMONITOR monitor) {
    if (monitor->m_name == std::string("HEADLESS-1")) {
        return;
    }
    if (isVerbose())
        printLog("Monitor PRE disconnect called with disabled monitor " + monitor->m_name);
    monitorLayoutChanging = true;
}

void onMonitorRemoved(PHLMONITOR monitor) {
    if (monitor->m_name == std::string("HEADLESS-1")) {
        return;
    }
    if (isVerbose())
        printLog("Monitor disconnect called with disabled monitor " + monitor->m_name);
    if (!currentlyEnabledMonitors(monitor).empty()) {
        monitorLayoutChanging = false;
        manager->invalidateAllLayouts();
        manager->deleteInvalidMonitorsOnAllVdesks(monitor);
        manager->applyCurrentVDesk();
        StickyApps::matchRules(stickyRules, manager);
    }
}

void onPreMonitorAdded(PHLMONITOR monitor) {
    if (monitor->m_name == std::string("HEADLESS-1")) {
        return;
    }
    if (isVerbose())
        printLog("Monitor PRE connect called with monitor " + monitor->m_name);
    monitorLayoutChanging = true;
}

void onMonitorAdded(PHLMONITOR monitor) {
    if (monitor->m_name == std::string("HEADLESS-1")) {
        return;
    }
    if (isVerbose())
        printLog("Monitor connect called with monitor " + monitor->m_name);
    monitorLayoutChanging = false;
    manager->invalidateAllLayouts();
    manager->deleteInvalidMonitorsOnAllVdesks();
    manager->applyCurrentVDesk();
    StickyApps::matchRules(stickyRules, manager);
}

void onConfigReloaded() {
    static auto* const PNOTIFYINIT = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, NOTIFY_INIT)->getDataStaticPtr();
    if (**PNOTIFYINIT && !notifiedInit) {
        HyprlandAPI::addNotification(PHANDLE, "Virtual desk Initialized successfully!", CHyprColor{0.f, 1.f, 1.f, 1.f}, 5000);
        notifiedInit = true;
    }
    static auto* const PVDESKNAMESCONF = (Hyprlang::STRING const*)(HyprlandAPI::getConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF))->getDataStaticPtr();
    auto               vdeskNamesConf  = std::string{*PVDESKNAMESCONF};
    parseNamesConf(vdeskNamesConf);
    finishWallpaperRuleReload();
    manager->loadLayoutConf();
}

void registerHyprctlCommands() {
    SHyprCtlCommand cmd;

    // Register printlayout
    cmd.name  = PRINTLAYOUT_DISPATCH_STR;
    cmd.fn    = printLayoutDispatch;
    cmd.exact = true;
    auto ptr  = HyprlandAPI::registerHyprCtlCommand(PHANDLE, cmd);
    if (!ptr)
        printLog(std::format("Failed to register hyprctl command: {}", PRINTLAYOUT_DISPATCH_STR));

    // Register printstate
    cmd.name  = PRINTSTATE_DISPATCH_STR;
    cmd.fn    = printStateDispatch;
    cmd.exact = true;
    ptr       = HyprlandAPI::registerHyprCtlCommand(PHANDLE, cmd);
    if (!ptr)
        printLog(std::format("Failed to register hyprctl command: {}", PRINTSTATE_DISPATCH_STR));

    // Register printdesk
    cmd.name  = PRINTDESK_DISPATCH_STR;
    cmd.fn    = printVDeskDispatch;
    cmd.exact = false;
    ptr       = HyprlandAPI::registerHyprCtlCommand(PHANDLE, cmd);
    if (!ptr)
        printLog(std::format("Failed to register hyprctl command: {}", PRINTDESK_DISPATCH_STR));

}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
    const std::string CLIENT_HASH     = __hyprland_api_get_client_hash();

    // ALWAYS add this to your plugins. It will prevent random crashes coming from
    // mismatched header versions.
    if (COMPOSITOR_HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[virtual-desktops] Mismatched headers! Can't proceed.", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[virtual-desktops] Version mismatch");
    }

    // Dispatchers
    HyprlandAPI::addDispatcherV2(PHANDLE, VDESK_DISPATCH_STR, virtualDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, LASTDESK_DISPATCH_STR, goLastVDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, PREVDESK_DISPATCH_STR, goPrevDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, NEXTDESK_DISPATCH_STR, goNextVDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, BACKCYCLE_DISPATCH_STR, cycleBackwardsDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, CYCLEVDESK_DISPATCH_STR, cycleVDeskDispatch);

    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETODESK_DISPATCH_STR, moveToDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETODESKSILENT_DISPATCH_STR, moveToDeskSilentDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETOLASTDESK_DISPATCH_STR, moveToLastDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETOLASTDESKSILENT_DISPATCH_STR, moveToLastDeskSilentDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETOPREVDESK_DISPATCH_STR, moveToPrevDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETOPREVDESKSILENT_DISPATCH_STR, moveToPrevDeskSilentDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETONEXTDESK_DISPATCH_STR, moveToNextDeskDispatch);
    HyprlandAPI::addDispatcherV2(PHANDLE, MOVETONEXTDESKSILENT_DISPATCH_STR, moveToNextDeskSilentDispatch);

    HyprlandAPI::addDispatcherV2(PHANDLE, RESET_VDESK_DISPATCH_STR, resetVDeskDispatch);

    // Configs
    HyprlandAPI::addConfigValue(PHANDLE, VIRTUALDESK_NAMES_CONF, Hyprlang::STRING{"unset"});
    HyprlandAPI::addConfigValue(PHANDLE, CYCLEWORKSPACES_CONF, Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, REMEMBER_LAYOUT_CONF, Hyprlang::STRING{REMEMBER_SIZE.c_str()});
    HyprlandAPI::addConfigValue(PHANDLE, NOTIFY_INIT, Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, VERBOSE_LOGS, Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, WALLPAPER_RENDER_CONF, Hyprlang::INT{0});

    // Keywords
    HyprlandAPI::addConfigKeyword(PHANDLE, STICKY_RULES_KEYW, parseStickyRule, Hyprlang::SHandlerOptions{});
    HyprlandAPI::addConfigKeyword(PHANDLE, WALLPAPER_RULE_KEYW, parseWallpaperRule, Hyprlang::SHandlerOptions{});

    onWorkspaceChangeHook   = Event::bus()->m_events.workspace.active.listen(onWorkspaceChange);
    onWindowOpenHook        = Event::bus()->m_events.window.open.listen(onWindowOpen);
    onConfigReloadedHook    = Event::bus()->m_events.config.reloaded.listen(onConfigReloaded);
    onPreMonitorAddedHook   = Event::bus()->m_events.monitor.preAdded.listen(onPreMonitorAdded);
    onPreMonitorRemovedHook = Event::bus()->m_events.monitor.preRemoved.listen(onPreMonitorRemoved);
    onMonitorAddedHook      = Event::bus()->m_events.monitor.added.listen(onMonitorAdded);
    onMonitorRemovedHook    = Event::bus()->m_events.monitor.removed.listen(onMonitorRemoved);
    onRenderStageHook       = Event::bus()->m_events.render.stage.listen(onRenderStage);

    registerHyprctlCommands();

    // Initialize first vdesk
    HyprlandAPI::reloadConfig();
    return {"virtual-desktops", "Virtual desktop like workspaces", "LevMyskin", "2.2.8"};
}
