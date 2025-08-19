## Summary of how the hyprland/workspaces module, particularly the window-rewrite feature, is configured in Waybar, based on the provided context:

Configuration Location:

The settings for the Hyprland workspaces module are defined within your main Waybar configuration file (e.g., ~/.config/waybar/config or config.jsonc).
The module is identified by the key "hyprland/workspaces".
A default configuration can be found in config.jsonc.
The test file hyprland-workspaces.json provides a specific example used in testing (test/config.cpp).
Core window-rewrite Configuration:

window-rewrite: (man/waybar-hyprland-workspaces.5.scd:23)
Type: object
Purpose: Defines rules to map window properties (class and/or title) to a specific string representation (often an icon).
Structure: A JSON object where keys are regular expressions matching window properties, and values are the replacement strings.
Rule Syntax: Keys can specify class<regex>, title<regex>, or both (e.g., "class<firefox> title<GitHub.*>").
Value Placeholders: Values can use {class} and {title} to insert the original window class or title.
Example (test/config/hyprland-workspaces.json:25):

// filepath: test/config/hyprland-workspaces.json// ...existing code..."window-rewrite": {  "title<Steam>": ""},// ...existing code...
window-rewrite-default: (man/waybar-hyprland-workspaces.5.scd:33)
Type: string
Default: ?
Purpose: The string representation used for windows that don't match any rule in window-rewrite.
Example (test/config/hyprland-workspaces.json:32): ""
format-window-separator: (man/waybar-hyprland-workspaces.5.scd:40)
Type: string
Default: " " (a single space)
Purpose: The separator string placed between multiple window representations when displayed within a workspace button.
Example (test/config/hyprland-workspaces.json:33): " "
Integration with format:

The generated window representations (from window-rewrite or window-rewrite-default, joined by format-window-separator) are typically displayed using the {windows} placeholder within the main format string for the workspace button.
Example (test/config/hyprland-workspaces.json:19): "format": "{icon} {windows}"
Other Related Settings (from example/docs):

format: Overall format string for the workspace button (man/waybar-hyprland-workspaces.5.scd:15).
format-icons: Maps workspace states/IDs to icons (man/waybar-hyprland-workspaces.5.scd:20).
sort-by: Method to sort workspaces (e.g., "number", "name") (src/modules/hyprland/workspaces.cpp:629, test/config/hyprland-workspaces.json:34).
persistent-workspaces: Assign workspaces to specific outputs (test/config/hyprland-workspaces.json:28).
Boolean flags like all-outputs, show-special, active-only, move-to-monitor control visibility and behavior (man/waybar-hyprland-workspaces.5.scd:48-65).
For full details on all configuration options, refer to the waybar-hyprland-workspaces.5.scd documentation file.

##  Task at hand 
Duplicate this config and this feature of showing icons for windows for workspaces in the niri config and the niri workspaces. the config can be named niri/workspaces. Under that we want to add "format": "<sub>{icon}</sub>{windows} ",
        "format-window-separator": " ",
       
        "window-rewrite-default": "",
        "window-rewrite": {
            "dolphin": "",
            "org.gnome.Nautilus": "",
            "thunar": "", }
            
           