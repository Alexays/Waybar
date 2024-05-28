waybar-menu(5)

# NAME

waybar - menu property

# OVERVIEW


Some modules support a 'menu', which allows to have a popup menu whan a defined
click is done over the module.

# PROPERTIES

A module that implements a 'menu' needs 3 properties defined in its config :

*menu*: ++
	typeof: string ++
	Action that popups the menu. The possibles actions are :

[- *Option*
:- *Description*
|[ *on-click*
:< When you left-click on the module
|[ *on-click-release*
:< When you release left button on the module
|[ *on-double-click*
:< When you double left click on the module
|[ *on-triple-click*
:< When you triple left click on the module
|[ *on-click-middle*
:< When you middle click on the module using mousewheel
|[ *on-click-middle-release*
:< When you release mousewheel button on the module
|[ *on-double-click-middle*
:< When you double middle click on the module
|[ *on-triple-click-middle*
:< When you triple middle click on the module
|[ *on-click-right*
:< When you right click on the module using
|[ *on-click-right-release*
:< When you release right button on the module
|[ *on-double-click-right*
:< When you double right click on the module
|[ *on-triple-click-right*
:< When you triple middle click on the module
|[ *on-click-backward*
:< When you  click on the module using mouse backward button
|[ *on-click-backward-release*
:< When you release mouse backward button on the module
|[ *on-double-click-backward*
:< When you double click on the module using mouse backward button
|[ *on-triple-click-backward*
:< When you triple click on the module using mouse backawrd button
|[ *on-click-forward*
:< When you  click on the module using mouse forward button
|[ *on-click-forward-release*
:< When you release mouse forward button on the module
|[ *on-double-click-forward*
:< When you double click on the module using mouse forward button
|[ *on-triple-click-forward*
:< When you triple click on the module using mouse forward button

*menu-file*: ++
	typeof: string ++
	Location of the menu descriptor file. There need to be an element of type
	GtkMenu with id *menu*.

*menu-actions*: ++
	typeof: array ++
	The actions corresponding to the buttons of the menu. The identifiers of
	each actions needs to exists as an id in the 'menu-file' for it to be linked
	properly.

# MENU-FILE

The menu-file is an `.xml` file representing a GtkBuilder. Documentation for it
can be found here : https://docs.gtk.org/gtk4/class.Builder.html

Here, it needs to have an element of type GtkMenu with id "menu". Eeach actions
in *menu-actions* are linked to elements in the *menu-file* file by the id of
the elements.

# EXAMPLE

Module config :
```
"custom/power": {
	"format" : "⏻ ",
		"tooltip": false,
		"menu": "on-click",
		"menu-file": "~/.config/waybar/power_menu.xml",
		"menu-actions": {
			"shutdown": "shutdown",
			"reboot": "reboot",
			"suspend": "systemctl suspend",
			"hibernate": "systemctl hibernate",
		},
},
```

~/.config/waybar/power_menu.xml :
```
<?xml version="1.0" encoding="UTF-8"?>
<interface>
  <object class="GtkMenu" id="menu">
	<child>
		<object class="GtkMenuItem" id="suspend">
			<property name="label">Suspend</property>
        </object>
	</child>
	<child>
        <object class="GtkMenuItem" id="hibernat">
			<property name="label">Hibernate</property>
        </object>
	</child>
    <child>
        <object class="GtkMenuItem" id="shutdown">
			<property name="label">Shutdown</property>
        </object>
    </child>
    <child>
      <object class="GtkSeparatorMenuItem" id="delimiter1"/>
    </child>
    <child>
		<object class="GtkMenuItem" id="reboot">
			<property name="label">Reboot</property>
		</object>
    </child>
  </object>
</interface>
```

# STYLING MENUS

- *menu*
  Style for the menu

- *menuitem*
  Style for items in the menu

# EXAMPLE:

```
menu {
	border-radius: 15px;
	background: #161320;
	color: #B5E8E0;
}
menuitem {
	border-radius: 15px;
}
```
