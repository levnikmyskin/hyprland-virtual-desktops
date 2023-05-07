# Virtual desktops for Hyprland ![hyprico](.github/hyprland.ico)
`virtual-desktops` is a plugin for the [Hyprland](https://github.com/hyprwm/Hyprland) compositor. `virtual-desktops` manages multiple screens workspaces as if they were a single virtual desktop.  

## What is this exactly?

In Hyprland, each screen has its own set of workspaces. For instance, say you have two monitors, with workspace 1 on screen 1 
and workspace 2 on screen 2:  
 - When you switch from workspace 1 to 2, Hyprland will simply focus your second screen;
 - If you switch to workspace 3, your active screen will go to workspace 3, whereas the other screen will stay on whichever workspace it is currently on.

You may think of a virtual desktop, instead, as a "single" 
workspace which extends across your screens (even though, internally, you will still have _n_ different workspaces on your _n_ monitors). If you've ever used KDE Plasma or Gnome with 
multiple screens, this plugin basically replicates that 
functionality.  
Taking the previous example:
 - You will be on virtual desktop 1. Let's say you open your web browser on your first screen and an IDE on your second screen;
 - When you switch to virtual desktop 2, both screens will switch to empty workspaces. Let's say here you open your email client and your favourite chat applicaiton;
 - If you switch back to virtual desktop 1, you will get back your web browser and the IDE on screen 1 and 2; and viceversa when you go back to virtual desktop 2.

## How does this work?

### It's just workspaces, really
Internally, this simply ties _n_ workspaces to your _n_ screens, for each virtual desktop. That is, on virtual desktop 1 you will always have workspace 1 on screen 1 and workspace 2 on screen 2;
on virtual desktop 2, you will always have workspace 3 on screen 1 and workspace 4 on screen 2, and so on.

**Notice**: screen 1 and screen 2 are not necessarily what you expect your first and second screen to be, i.e., screen 1 is not necessarily your left screen, and screen 2 is not necessarily your right screen.

### Hyprctl dispatchers

This plugin exposes a few hyprctl dispatchers:

| Dispatcher | descritpion | type | example|
|------------|-------------|------|--------|
| vdesk [vdesk]      | Changes to virtual desktop `vdesk` | see below | `vdesk 3` or `vdesk coding`|
| prevdesk | Changes to previous virtual desktop | `none` | `prevdesk`|
| printdesk (vdesk)| Prints to Hyprland log the specified vdesk or the currently active vdesk* (if no argument is given) | optional vdesk, see below | `printdesk` or `printdesk 2` or `printdesk coding`|

\*`printdesk` currently prints to the active Hyprland session log, thus probably not really useful. 

For `vdesk` names, you can use:
 - ID: e.g., `1`, `2` etc;
 - Name: e.g., `coding`, `internet`, `mail and chats`

If a `vdesk` with a given ID or name does not exist, it'll be created on the fly. If you give a (non configured, see [below](#configuration-values)) 
name, it will be assigned to the next available vdesk id: the virtual-desktops 
plugin will remember this association even if Hyprland kills the related workspaces. 

#### Don't mix with Hyprland native workspaces
I wouldn't mix this with any of Hyprland native workspaces functionality. For instance, instead of using `hyprctl dispatch workspace n` use 
`hyprctl dispatch vdesk n`, even if you have no secondary screen connected at the moment (the behaviour would be identical to native workspaces). Also, I would REMOVE
any workspace related configuration, such as `wsbind`. If you want to leverage [workspace-specific rules](https://wiki.hyprland.org/Configuring/Workspace-Rules/), you can: workspaces are always assigned 
to the same vdesk given the same number of monitors. For instance:
 - Given two monitors:
   - vdesk 1 has workspaces 1 and 2;
   - vdesk 2 has workspaces 3 and 4, and so on;
 - Given three monitors:
   - vdesk 1 has workspaces 1, 2 and 3;
   - vdesk 2 has workspaces 4, 5 and 6, and so on.
 - Given four monitors...

The vdesk a workspace will end up to is easily computed by doing `ceil(workspace_id / n_monitors)`. You know where I'm going with this one...you can easily script it.


### Configuration values

This plugin exposes a few configuration options, under the `plugin:virtual-desktops:` category, namely:

| Name | type | example|
|------|------|--------|
| names | string, see below| `1:coding, 2:internet, 3:mail and chats`|
| cycleworkspaces | `0` or `1`| `1`|

* The `names` config option maps virtual desktop IDs to a name (you can then use this with the hyprctl [dispatcher](#hyprctl-dispatchers));
* If `cycleworkspaces` is set to `1`, and you switch to the currently active virtual desktop, this swaps the workspaces of your two monitors (see hyprctl [swapactiveworkspaces](https://wiki.hyprland.org/Configuring/Dispatchers/#list-of-dispatchers)). 
THIS CURRENTLY DOES NOT WORK WITH MORE THAN 2 MONITORS. If you need this feature, please feel welcome to submit a PR ^^.


#### Example config 
```ini
plugin {
    virtual-desktops {
        names = 1:coding, 2:internet, 3:mail and chats 
        cycleworkspaces = 1
    }
}
```


## Install
In order to use plugins, you should compile Hyprland yourself. See [Hyprland Wiki#Using Plugins](https://wiki.hyprland.org/Plugins/Using-Plugins/).  

You can use:  
```bash
HYPRLAND_HEADERS=path/to/hyprlandrepo make all
```  
this will compile and copy the compiled `.so` plugin in the `$HOME/.local/share/hyprload/plugins/bin` path.  
You can also use `make virtual-desktops.so` to compile the plugin in the repo directory.

Once compiled, you can tell Hyprland to load the plugin as described in the Hyprland wiki.


### Thanks to
[split-workspaces](https://github.com/Duckonaut/split-monitor-workspaces/), from which I borrowed the Makefile, 
and the general idea of how to write Hyprland plugins.
