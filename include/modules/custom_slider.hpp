#pragma once

#include <json/json.h>

#include <string>
#include <vector>

#include "ASlider.hpp"
#include "util/custom_source.hpp"

namespace waybar::modules {

class CustomSlider : public ASlider {
 public:
  CustomSlider(const std::string& name, const std::string& id, const Json::Value& config,
               const std::string& output_name);
  virtual ~CustomSlider() = default;

  auto update() -> void override;
  auto refresh(int signal) -> void override;

 protected:
  void onCommit(int value) override;

 private:
  const std::string name_;
  std::string on_change_;
  std::vector<std::string> prev_classes_;
  util::CustomSource source_;
};

}  // namespace waybar::modules
