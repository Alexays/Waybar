#include "modules/prayer_timer.hpp"

#include <time.h>

waybar::modules::prayer_timer::prayer_timer(const std::string& id, const Json::Value& config)
    : ALabel(config, "prayer_timer", id, "{next}", 1) {

    file_path_ = config_["file-path"].asString();

        thread_ = [this]{
            dp.emit();
            auto now = std::chrono::system_clock::now();
            /* difference with projected wakeup time */
            auto diff = now.time_since_epoch() % interval_;
            /* sleep until the next projected time */
            thread_.sleep_for(interval_ - diff);
        };
    }
void waybar::modules::prayer_timer::SecondsToString(int seconds, char *buffer, int buffer_size) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    snprintf(buffer, buffer_size, "%02d:%02d:%02d", hours, minutes, secs);
}

auto waybar::modules::prayer_timer::update() -> void {
    std::ifstream infile(file_path_);
    char* arr[6] = {"Fajr ", "Sunrise ", "Dhuhr ", "Asr ", "Maghrib ", "Isha "};
    int idx = 0, rem = 0;
    infile >> idx;
    infile >> rem;
    infile.close();
    char buf[30];
    strcpy(buf, arr[idx]);
    char temp[10];
    SecondsToString(rem, temp, 10);
    strcat(buf, temp);
    label_.set_markup(buf);
    // Call parent update
    ALabel::update();
}
