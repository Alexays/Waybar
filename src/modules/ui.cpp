#include "modules/ui.hpp"

#include <gtkmm/builder.h>
#include <spdlog/spdlog.h>

#include <util/command.hpp>

waybar::modules::UI::UI(const std::string& name, const std::string& id, const Json::Value& config)
    : AModule(config, "ui-" + name, id, false, false) {
  if (config_["file-path"].isString()) {
    Glib::RefPtr<Gtk::Builder> builder{
        Gtk::Builder::create_from_file(config_["file-path"].asString())};
    Glib::RefPtr<Gtk::Widget> uiWg{
        Glib::RefPtr<Gtk::Widget>::cast_dynamic(builder->get_object(name_))};

    if (uiWg) {
      uiWg->set_name(name_);
      if (!id.empty()) {
        uiWg->get_style_context()->add_class(id);
      }
      uiWg->get_style_context()->add_class(MODULE_CLASS);

      Glib::RefPtr<Gio::SimpleActionGroup> actionGroup{Gio::SimpleActionGroup::create()};
      Glib::RefPtr<Gio::SimpleAction> action{actionGroup->add_action_with_parameter(
          "doAction", Glib::VARIANT_TYPE_STRING, [this](const Glib::VariantBase& param) {
            assert(param.is_of_type(Glib::VARIANT_TYPE_STRING));
            waybar::util::command::res res = waybar::util::command::exec(
                Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(param).get(), "TLP");
          })};

      uiWg->insert_action_group(name_, actionGroup);
      event_box_.add(*uiWg.get());
    } else {
      spdlog::error("UI: object id \"{}\" is not found at \"{}\"", name_,
                    config_["file-path"].asString());
    }
  }
}
