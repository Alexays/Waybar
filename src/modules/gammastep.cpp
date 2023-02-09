#include "include/modules/gammastep.hpp"

waybar::modules::GammaButton::GammaButton() : Gtk::Button((Glib::ustring)(SUN_STRING)) {
	this->signal_clicked().connect(
		sigc::mem_fun(*this, &GammaButton::handle_clicked)
	);
}

waybar::modules::GammaButton::~GammaButton() {

}

void waybar::modules::GammaButton::handle_clicked() {
	if (this->get_label() == (Glib::ustring)(SUN_STRING)) {
		this->set_label((Glib::ustring)(MOON_STRING));
		// system(this->command_start.c_str());
		spdlog::info("gammastep activated");
	} 
	else {
		this->set_label((Glib::ustring)(SUN_STRING));
		// system(this->command_reset.c_str());
		spdlog::info("gammastep killed");
	}
}

waybar::modules::Settings::Settings() :
	app_(Gtk::Application::create("GammaSettings", Gio::APPLICATION_FLAGS_NONE)),
	box_(Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0)),
	label_("HELLO WORLD") {
	
	box_.pack_start(label_);
	window_.add(box_);
	window_.show_all_children();
}

waybar::modules::Settings::~Settings() {

}

int waybar::modules::Settings::run() {
	return app_->run(window_);
}

waybar::modules::SettingsButton::SettingsButton() : 
	Gtk::Button((Glib::ustring)(SETTINGS_STRING)) {

	this->signal_clicked().connect(
		sigc::mem_fun(*this, &SettingsButton::handle_clicked)
	);
}

waybar::modules::SettingsButton::~SettingsButton() {

}

void waybar::modules::SettingsButton::handle_clicked() {
	spdlog::info("SETTINGS");
	settings_.run();
}

waybar::modules::Gammastep::Gammastep(
	const waybar::Bar &bar, const std::string& id, const Json::Value& config) :	
		ALabel(config, "gammastep", id, "", 60, false, true, false),
		box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
	
	box_.set_homogeneous(false);
	box_.pack_start(gamma_button);
	box_.pack_end(settings_button);

	event_box_.remove();
	event_box_.add(box_);
} 

waybar::modules::Gammastep::~Gammastep() {

}

void waybar::modules::Gammastep::update() {

}