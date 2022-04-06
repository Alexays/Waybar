#include <filesystem>

#include "modules/cpu.hpp"

std::vector<std::tuple<size_t, size_t>> waybar::modules::Cpu::parseCpuinfo() {
  const std::string data_dir_ = "/proc/stat";
  std::ifstream info(data_dir_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + data_dir_);
  }
  std::vector<std::tuple<size_t, size_t>> cpuinfo;
  std::string line;
  while (getline(info, line)) {
    if (line.substr(0, 3).compare("cpu") != 0) {
      break;
    }
    std::stringstream sline(line.substr(5));
    std::vector<size_t> times;
    for (size_t time = 0; sline >> time; times.push_back(time))
      ;

    size_t idle_time = 0;
    size_t total_time = 0;
    if (times.size() >= 4) {
      idle_time = times[3];
      total_time = std::accumulate(times.begin(), times.end(), 0);
    }
    cpuinfo.emplace_back(idle_time, total_time);
  }
  return cpuinfo;
}

std::vector<float> waybar::modules::Cpu::parseCpuFrequencies() {
  const std::string file_path_ = "/proc/cpuinfo";
  std::ifstream info(file_path_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }
  std::vector<float> frequencies;
  std::string line;
  while (getline(info, line)) {
    if (line.substr(0, 7).compare("cpu MHz") != 0) {
      continue;
    }

    std::string frequency_str = line.substr(line.find(":") + 2);
    float frequency = std::strtol(frequency_str.c_str(), nullptr, 10);
    frequencies.push_back(frequency);
  }
  info.close();

  if (frequencies.size() <= 0) {
    std::string cpufreq_dir = "/sys/devices/system/cpu/cpufreq";
    if (std::filesystem::exists(cpufreq_dir)) {
      std::vector<std::string> frequency_files = {"/cpuinfo_min_freq", "/cpuinfo_max_freq"};
      for (auto& p : std::filesystem::directory_iterator(cpufreq_dir)) {
        for (auto freq_file : frequency_files) {
          std::string freq_file_path = p.path().string() + freq_file;
          if (std::filesystem::exists(freq_file_path)) {
            std::string freq_value;
            std::ifstream freq(freq_file_path);
            if (freq.is_open()) {
              getline(freq, freq_value);
              float frequency = std::strtol(freq_value.c_str(), nullptr, 10);
              frequencies.push_back(frequency / 1000);
              freq.close();
            }
          }
        }
      }
    }
  }

  return frequencies;
}
