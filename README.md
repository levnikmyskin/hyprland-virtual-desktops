# Virtual desktops for Hyprland ![hyprico](.github/hyprland.ico)
`virtual-desktops` is a plugin for the [Hyprland](https://github.com/hyprwm/Hyprland) compositor. `virtual-desktops` manages multiple screens workspaces as if they were a single virtual desktop.  

## Table of contents
- [Virtual desktops for Hyprland ](#virtual-desktops-for-hyprland-)
  - [Table of contents](#table-of-contents)
  - [What is this exactly?](#what-is-this-exactly)
  - [How does this work?](#how-does-this-work)
    - [It's just workspaces, really](#its-just-workspaces-really)
    - [Hyprctl dispatchers](#hyprctl-dispatchers)
      - [Mix with Hyprland native workspaces](#mix-with-hyprland-native-workspaces)
    - [Hyprctl commands](#hyprctl-commands)
    - [Hyprland keywords](#hyprland-keywords)
      - [Syntax](#syntax)
      - [Examples](#examples)
    - [Configuration values](#configuration-values)
      - [Example config](#example-config)
  - [Layouts](#layouts)
      - [Example](#example)
    - [Layouts are cached and restored if you disconnect/reconnect monitors](#layouts-are-cached-and-restored-if-you-disconnectreconnect-monitors)
      - [Example](#example-1)
    - [Choosing how to remember, or choosing to forget](#choosing-how-to-remember-or-choosing-to-forget)
      - [Example](#example-2)
  - [Install](#install)
    - [Installing on NixOS with home—manager](#installing-on-nixos-with-homemanager)
  - [Help, Hyprland is being weird!](#help-hyprland-is-being-weird)
    - [It's actually the plugin 😱](#its-actually-the-plugin-)
  - [Thanks to](#thanks-to)


## What is this exactly?

In Hyprland, each screen has its own set of workspaces. For instance, say you have two monitors, with workspace 1 on screen 1 
and workspace 2 on screen 2:  
 - When you switch from workspace 1 to 2, Hyprland will simply focus your second screen;
 - If you switch to workspace 3, your active screen will go to workspace 3, whereas the other screen will stay on whichever workspace it is currently on.

You may think of a virtual desktop, instead, as a "single" 
workspace which extends across your screens (even though, internally, you will still have _n_ different workspaces on your _n_ monitors). If you've ever used KDE Plasma (or Gnome, I think) with 
multiple screens, this plugin basically replicates that 
functionality.  
Taking the previous example:
 - You will be on virtual desktop 1. Let's say you open your web browser on your first screen and an IDE on your second screen;
 - When you switch to virtual desktop 2, both screens will switch to empty workspaces. Let's say here you open your email client and your favourite chat application;
 - If you switch back to virtual desktop 1, you will get back your web browser and the IDE on screen 1 and 2; and viceversa when you go back to virtual desktop 2.
 
## How does this work?

### It's just workspaces, really
Internally, this simply ties _n_ workspaces to your _n_ screens, for each virtual desktop. That is, on virtual desktop 1 you will have workspace 1 on screen 1 and workspace 2 on screen 2;
on virtual desktop 2, you will have workspace 3 on screen 1 and workspace 4 on screen 2, and so on.

However, if you focus another workspace on a given virtual desktop, the plugin will remember this and you will keep this layout (see [Layouts](#Layouts)). 

**Notice**: screen 1 and screen 2 are not necessarily what you expect your first and second screen to be, e.g., screen 1 is not necessarily your left screen, and screen 2 is not necessarily your right screen.

### Hyprctl dispatchers

This plugin exposes a few hyprctl dispatchers:

| Dispatcher | description | type | example|
|------------|-------------|------|--------|
| vdesk [vdesk]      | Changes to virtual desktop `vdesk` | see below | `vdesk 3` or `vdesk coding`|
| lastdesk | Changes to last visited virtual desktop | `none` | `lastdesk`|
| movetodesk vdesk(, window) | Moves the active/selected window to the specified `vdesk` | `vdesk`, optional window, see below | `movetodesk 2` or `movetodesk 2,title:kitty` |
| movetodesksilent vdesk(, window) | same as `movetodesk`, but doesn't switch to desk | same as above | same as above |
| movetolastdesk  (window) | Moves the active/selected window to the last visited `vdesk` | optional window | `movetolastdesk` or `movetolastdesk title:kitty` |
| movetolastdesksilent (window) | same as `movetolastdesk`, but doesn't switch to desk | same as above | same as above |
| movetoprevdesk (cycleback)(,window) | Moves the active/selected window to the previous `vdesk`. If `cycleback == 1 and vdesk == 1`, moves to vdesk with max id | optional 1 for true, optional window | `movetoprevdesk 1` or `movetodesk ,title:kitty` |
| movetoprevdesksilent (cycleback)(,window) | same as `movetoprevdesk`, but doesn't switch to desk | same as above | same as above |
| movetonextdesk (cycle)(, window) | Moves the active/selected window to the next `vdesk`. If `cycle == 1 and vdesk == max`, moves to vdesk 1 | optional 1 for true, optional window | `movetonextdesk 1` or `movetonextdesk ,title:kitty` |
| movetonextdesksilent (cycle)(, window) | same as `movetonextdesk`, but doesn't switch to desk | same as above | same as above |
| vdeskreset (vdesk) | reset layouts on `vdesk` or on all vdesks if no argument is given (see [Layouts](#Layouts))  | optional vdesk, see below | `vdeskreset` or `vdeskreset 2` or `vdeskreset coding` |
| prevdesk | go to previous vdesk. If less than 1, will simply execute `vdesk 1` | `none` | `prevdesk` |
| nextdesk | go to next vdesk. Creates it if it doesn't exist | `none` | `nextdesk` |
| backcyclevdesks | backward cycle between currently existing vdesks. Goes back to vdesk with max id when at vdesk 1 | `none` | `backcyclevdesks` |
| cyclevdesks | cycle between currently existing vdesks. Goes back to vdesk 1 if next vdesk does not exist | `none` | `cyclevdesks` |


> BREAKING v2.1.0: `prevdesk` dispatcher was renamed to `lastdesk`. `prevdesk` has a new functionality: it goes to the previous desk. If you were using `prevdesk`, please update your config. 

For `vdesk` names, you can use:
 - ID: e.g., `1`, `2` etc;
 - Name: e.g., `coding`, `internet`, `mail and chats`

If a `vdesk` with a given ID or name does not exist, it'll be created on the fly. If you give a (non configured, see [below](#configuration-values)) 
name, it will be assigned to the next available vdesk id: the virtual-desktops 
plugin will remember this association even if Hyprland kills the related workspaces. 
  
The `movetodesk` and `movetodesksilent` dispatchers work similarly to 
Hyprland's `movetoworkspace` and `movetoworkspacesilent` dispatchers. See [Hyprland's wiki](https://wiki.hyprland.org/Configuring/Dispatchers/#list-of-dispatchers). Of course, make sure to use the `vdesk` syntax above instead of Hyprland's.

#### Mix with Hyprland native workspaces 
You can use `hyprctl dispatch vdesk n`, even if you have 
no secondary screen connected at the moment (the behaviour would be identical to native workspaces). Also, I would REMOVE
any workspace related configuration, such as `wsbind`. If you want to leverage [workspace-specific rules](https://wiki.hyprland.org/Configuring/Workspace-Rules/), you can: workspaces are always assigned 
to the same vdesk given the same number of monitors, unless you focus (e.g. with hyprctl) another workspace (see [Layouts](#Layouts)). For instance:
 - Given two monitors:
   - vdesk 1 has workspaces 1 and 2;
   - vdesk 2 has workspaces 3 and 4, and so on;
 - Given three monitors:
   - vdesk 1 has workspaces 1, 2 and 3;
   - vdesk 2 has workspaces 4, 5 and 6, and so on.
 - Given four monitors...

The vdesk a workspace will end up to is easily computed by doing `ceil(workspace_id / n_monitors)`. You know where I'm going with this one...you can easily script it.

### Hyprctl commands
Since version 2.2, this plugin exposes a couple of `hyprctl` commands. That is, you can use them by calling `hyprctl {command} {args}`.  
**NOTICE**: some of these used to be dispatchers.

| Command | description | args | example|
|------------|-------------|------|--------|
| printdesk (vdesk)| Prints to Hyprland log the specified vdesk or the currently active vdesk* (if no argument is given) | optional vdesk, see [above](#hyprctl-dispatchers) | `hyprctl printdesk` or `hyprctl printdesk 2` or `hyprctl printdesk coding`|
| printlayout | print to Hyprland logs the current layout | `none` | `hyprctl printlayout` |


### Hyprland keywords
Since version 2.2, this plugin exposes one keyword: `stickyrule`.  
A sticky rule is composed of a window identifier and a vdesk identifier.  
A window matched by a sticky rule will be moved to the matched vdesk:
  1. When the window is created (similar to [Hyprland's `workspace` windowrule](https://wiki.hyprland.org/Configuring/Window-Rules/#window-rules-v2), but with virtual desks);
  2. Every time a monitor is connected/disconnected.
  
**BE CAREFUL**:
  1. **NOT** to mix this with Hyprland's `workspace` windowrule (it wouldn't make sense right?);
  2. This is not a plugin config, but an Hyprland keyword. Place it in the top level of Hyprland's config (i.e., where you'd put windowrules too).
  
#### Syntax
```bash
stickyrule = window,vdesk
```
  - `window` identifier has the same syntax as [Hyprland's windowrule window](https://wiki.hyprland.org/Configuring/Window-Rules/#window-rules-v2) identifier;
  - `vdesk` identifier has the same syntax specified above.

#### Examples
`stickyrule = class:^(kittysticky)$,3`  
`stickyrule = title:thunderbird,mail`



### Configuration values

This plugin exposes a few configuration options, under the `plugin:virtual-desktops:` category, namely:

| Name | description | type | example|
|------|-------------|------|--------|
| names | map a vdesk id with a name | map[int:string], see below| `names = 1:coding, 2:internet, 3:mail and chats`|
| cycleworkspaces | if set to 1 and switching to the currently active vdesk, workspaces will be swapped between your monitors (see [swapactiveworkspaces](https://wiki.hyprland.org/Configuring/Dispatchers/#list-of-dispatchers))| `0` or `1`| `cycleworkspaces = 1`|
| rememberlayout | chooses how layouts should be remembered (see [Layouts](#Layouts)), defaults to `size` | `none`, `size` or `monitors` | `remember = size` |
| notifyinit | chooses whether to display the startup notification, defaults to 1 | `0` or `1` | `notifyinit = 0` |
| verbose_logging | whether to log more stuff, defaults to 0 | `0` or `1` | `verbose_logging = 0` |

* The `names` config option maps virtual desktop IDs to a name (you can then use this with the hyprctl [dispatchers](#hyprctl-dispatchers));
* `cycleworkspaces`:  THIS CURRENTLY DOES NOT WORK WITH MORE THAN 2 MONITORS. If you need this feature, please feel welcome to submit a PR ^^.


#### Example config 
```ini
stickyrule = class:^(kittysticky)$,3
stickyrule = title:thunderbird,mail

plugin {
    virtual-desktops {
        names = 1:coding, 2:internet, 3:mail and chats 
        cycleworkspaces = 1
        rememberlayout = size
        notifyinit = 0
        verbose_logging = 0
    }
}
```

## Layouts
Version 2.0 of this plugin introduced the concept of a *layout*, with the meaning of "a specific combination of workspaces on a (more or less) specific combination of monitors".  
In other words, `virtual-desktops` remembers if you focused another workspace on your vdesk, even if you switch to another vdesk and then come back to this one

#### Example
Say you have 2 monitors A and B, and you're on vdesk 1:
 - On monitor A you have workspace 1, and on monitor B you have workspace 2;
 - Now, say you focus workspace 4 with `hyprctl dispatch workspace 4` on monitor B. 
 - If you switch to vdesk 2 and back to vdesk 1, you will see workspace 4 on monitor B instead of workspace 2.  
  
**Notice** that, in this case, workspace 4 would also be shown on vdesk 2.

### Layouts are cached and restored if you disconnect/reconnect monitors
Internally, every vdesk will cache all the previous layouts. Once a monitor is connected/disconnected, the plugin will try to look for a previously existing layout for this configuration
and it will apply it.

#### Example
Continuing from the previous example, say you now disconnect monitor B and then you reconnect it, while being on vdesk 1:
 - the previous layout will be matched and you will get back workspace 1 on monitor A and workspace 4 on monitor B.

This would work also if instead of disconnecting and reconnecting monitor B, you connected a third monitor C and then disconnect it.


### Choosing how to remember, or choosing to forget
Version 2.0 also introduced the `rememberlayout` config option: with this option, we can choose if we want to match a layout by number of monitors (`size`) or by unique monitor descriptions (`monitors`). There is also the third and final option to not remember layouts at all (`none`).


#### Example
Continuing from the previous example. Say we now disconnect monitor B and connect monitor C: our connected monitors are A and C.

 - if `rememberlayout = size` (the default), the existing layout will still be matched, we will have workspace 1 on monitor A, workspace 4 on monitor C;
 - if `rememberlayout = monitors`, a new layout will be created with defaults: workspace 1 on monitor A, workspace 2 on monitor C;
 - if `rememberlayout = none`, same as above.

If we now disconnect monitor C and reconnect monitor B: our connected monitors are A and B.

 - if `rememberlayout = size` (the default), the existing layout will be matched, we will have workspace 1 on monitor A, workspace 4 on monitor B;
 - if `rememberlayout = monitors`, the existing layout will be matched, we will have workspace 1 on monitor A, workspace 4 on monitor B;
 - if `rememberlayout = none`, a new layout will be created with defaults: workspace 1 on monitor A, workspace 2 on monitor C.


## Install
In order to use plugins, you should compile Hyprland yourself. See [Hyprland Wiki#Using Plugins](https://wiki.hyprland.org/Plugins/Using-Plugins/).  

You can use:  
```bash
HYPRLAND_HEADERS=path/to/hyprlandrepo make all
```  
this will compile and copy the compiled `.so` plugin in the `$HOME/.local/share/hyprload/plugins/bin` path (useful if you use [hyprload](https://github.com/Duckonaut/hyprload)).  
You can also use `make virtual-desktops.so` to output the compiled plugin in the repo directory.

Once compiled, you can tell Hyprland to load the plugin as described in the [Hyprland wiki](https://wiki.hyprland.org/Plugins/Using-Plugins/#installing--using-plugins).


### Installing on NixOS with home—manager

Here is an example flake that you can modify to add hyprland-virtual-desktops to your configuration
```nix
# flake.nix

{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
   
    hyprland = {
      url = "github:hyprwm/Hyprland";
      follows = "hyprland-virtual-desktops/hyprland"; # To make sure we run the same version of hyprland that the plugin was built against
    };
    hyprland-virtual-desktops.url = "github:levnikmyskin/hyprland-virtual-desktops";
    
  };

  outputs = { nixpkgs, home-manager, hyprland, hyprland-virtual-desktops, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      homeConfigurations."user@hostname" = home-manager.lib.homeManagerConfiguration {
        pkgs = nixpkgs.legacyPackages.x86_64-linux;

        # You can optionally move this module to its own .nix file and source it
        # here if you want to modularise your configuration
        modules = [
          hyprland.homeManagerModules.default
          {
            wayland.windowManager.hyprland = {
              enable = true;
              package = hyprland.packages.${pkgs.system}.hyprland;
              plugins = [
                hyprland-virtual-desktops.packages.${pkgs.system}.virtual-desktops
              ];
              # extraConfig is a string that becomes hyprland.conf
              extraConfig = ''
                stickyrule = class:^(kittysticky)$,3
                stickyrule = title:thunderbird,mail

                plugin {
                    virtual-desktops {
                        names = 1:coding, 2:internet, 3:mail and chats 
                        cycleworkspaces = 1
                        rememberlayout = size
                        notifyinit = 0
                        verbose_logging = 0
                    }
                }
              '' + ''
                # your other configuration for hyprland
              '';
            };
          }
          # ...
          # NOTE:
          # You will want to enable the Hyprland module in your NixOS configuration
          # too, since that also enables critical components like xdg-desktop-portal,
          # xwayland, polkit, etc
          # 
          # # Have this somewhere in your NixOS configuration
          # programs.hyprland = {
          #   enabled = true;
          #   package = inputs.hyprland.packages.${pkgs.system}.hyprland;
          # };
        ];
      };
    };
}
```

## Help, Hyprland is being weird!
I've noticed that, sometimes, when disconnecting or reconnecting monitors, there might be weird artifacts or similar. Try running:  
`hyprctl reload`  
  
### It's actually the plugin 😱
If instead you're seeing weird behaviour with the plugin itself, remember you can always run:  
`hyprtl dispatch vdeskreset`

## Thanks to
[split-workspaces](https://github.com/Duckonaut/split-monitor-workspaces/), from which I borrowed the Makefile, 
and the general idea of how to write Hyprland plugins.
