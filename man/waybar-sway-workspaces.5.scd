waybar-sway-workspaces(5)

# NAME

waybar - sway workspaces module

# DESCRIPTION

The *workspaces* module displays the currently used workspaces in Sway.

# CONFIGURATION

Addressed by *sway/workspaces*

*all-outputs*: ++
    typeof: bool ++
    default: false ++
    If set to false, workspaces will only be shown on the output they are on. If set to true all workspaces will be shown on every output.

*format*: ++
    typeof: string ++
    default: {value} ++
    The format, how information should be displayed.

*format-icons*: ++
    typeof: array ++
    Based on the workspace name and state, the corresponding icon gets selected. See *icons*.

*disable-scroll*: ++
    typeof: bool ++
    default: false ++
    If set to false, you can scroll to cycle through workspaces. If set to true this behaviour is disabled.

*disable-click*: ++
    typeof: bool ++
    default: false ++
    If set to false, you can click to change workspace. If set to true this behaviour is disabled.

*smooth-scrolling-threshold*: ++
    typeof: double ++
    Threshold to be used when scrolling.

*disable-scroll-wraparound*: ++
    typeof: bool ++
    default: false ++
    If set to false, scrolling on the workspace indicator will wrap around to the first workspace when reading the end, and vice versa. If set to true this behavior is disabled.

*enable-bar-scroll*: ++
    typeof: bool ++
    default: false ++
    If set to false, you can't scroll to cycle throughout workspaces from the entire bar. If set to true this behaviour is enabled.

*disable-markup*: ++
    typeof: bool ++
    default: false ++
    If set to true, button label will escape pango markup.

*current-only*: ++
    typeof: bool ++
    default: false ++
    If set to true. Only focused workspaces will be shown.

*persistent_workspaces*: ++
    typeof: json (see below) ++
    default: empty ++
    Lists workspaces that should always be shown, even when non existent

*on-update*: ++
    typeof: string ++
    Command to execute when the module is updated.

*disable-auto-back-and-forth*: ++
    typeof: bool ++
    Whether to disable *workspace_auto_back_and_forth* when clicking on workspaces. If this is set to *true*, clicking on a workspace you are already on won't do anything, even if *workspace_auto_back_and_forth* is enabled in the Sway configuration.

# FORMAT REPLACEMENTS

*{value}*: Name of the workspace, as defined by sway.

*{name}*: Number stripped from workspace value.

*{icon}*: Icon, as defined in *format-icons*.

*{index}*: Index of the workspace.

# ICONS

Additional to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string matches is found.
- *urgent*: Will be shown, when workspace is flagged as urgent
- *focused*: Will be shown, when workspace is focused
- *persistent*: Will be shown, when workspace is persistent one.

# PERSISTENT WORKSPACES

Each entry of *persistent_workspace* names a workspace that should always be shown.
Associated with that value is a list of outputs indicating *where* the workspace should be shown,
an empty list denoting all outputs.

```
"sway/workspaces": {
    "persistent_workspaces": {
        "3": [], // Always show a workspace with name '3', on all outputs if it does not exists
        "4": ["eDP-1"], // Always show a workspace with name '4', on output 'eDP-1' if it does not exists
        "5": ["eDP-1", "DP-2"] // Always show a workspace with name '5', on outputs 'eDP-1' and 'DP-2' if it does not exists
    }
}
```

n.b.: the list of outputs can be obtained from command line using *swaymsg -t get_outputs*

# EXAMPLES

```
"sway/workspaces": {
    "disable-scroll": true,
    "all-outputs": true,
    "format": "{name}: {icon}",
    "format-icons": {
        "1": "",
        "2": "",
        "3": "",
        "4": "",
        "5": "",
        "urgent": "",
        "focused": "",
        "default": ""
    }
}
```

# Style

- *#workspaces button*
- *#workspaces button.visible*
- *#workspaces button.focused*
- *#workspaces button.urgent*
- *#workspaces button.persistent*
- *#workspaces button.current_output*
- *#workspaces button#sway-workspace-${name}*
