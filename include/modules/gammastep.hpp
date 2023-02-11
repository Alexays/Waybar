#pragma once

#ifndef GAMMASTEP_H
#define GAMMASTEP_H

#include "ALabel.hpp"
#include "bar.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>

#include "gtk-layer-shell.h"
#include <gtkmm/application.h>
#include <gtkmm/button.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/box.h>
#include <gtkmm.h>
#include <glibmm/refptr.h>
#include <glib.h>


#define MOON_STRING "\xf0\x9f\x8c\x99"
#define SUN_STRING "\xf0\x9f\x8c\x9e"
#define SETTINGS_STRING "⚙"

namespace waybar::modules {

class Settings {
public:
	struct settings_margins {
		uint top = 0;
		uint right = 0;
		uint bottom = 0;
		uint left = 0;
	};
	struct settings_anchors {
		bool top = true;
		bool right = true;
		bool bottom = false;
		bool left = false;
	};

	Settings(const Json::Value&);
	virtual ~Settings();
	static Settings* create(const Json::Value&);

	int run();
	void close();
protected:
	void set_margins(const Json::Value&);
	void set_anchors(const Json::Value&);
private:
	Glib::RefPtr<Gtk::Application> app_;
	Gtk::Window window_;
	Gtk::Box box_;
	Gtk::Label label_;
	const Json::Value& config_;
};

class SettingsButton : public Gtk::ToggleButton {
public:
	SettingsButton(const Json::Value&);
	virtual ~SettingsButton();
	void handle_toggled();
private:
	Settings* settings_;
	const Json::Value& config_;
};

class GammaButton : public Gtk::ToggleButton {
public:
	GammaButton(const Json::Value&);
	virtual ~GammaButton();
	void handle_toggled();
private:
	const std::string command_reset = "killall gammastep";
	const std::string command_start = "gammastep -m wayland -O 3000 &";
	const Json::Value& config_;
};
	
class Gammastep : public ALabel {
public:
	Gammastep(const waybar::Bar&, const std::string&, const Json::Value&);
	virtual ~Gammastep();
	auto update() -> void;
private:
	Gtk::Box box_;
	GammaButton gamma_button;
	SettingsButton settings_button;
};

} // namespace waybar::module 

#endif // ! GAMMASTEP_H