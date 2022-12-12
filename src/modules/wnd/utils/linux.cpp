#include "modules/wnd/utils/process.hpp"

#ifdef __linux__

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "modules/wnd/cpu/process_cpu_linux.hpp"
#include "modules/wnd/memory/process_memory_linux.hpp"
#include "modules/wnd/system/cpu.hpp"

namespace {
static inline std::string ltrim(std::string s) {
  s.erase(s.begin(),
          std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));

  return s;
}

class Processes {
 private:
  struct Process {
    std::string name;
    std::string pid;
    std::string ppid;

    wnd::Memory memory;

    long cpu;
    long cpuLast;
  };

  std::vector<struct Process> _processes;
  long cpuLast;

  std::pair<std::string, std::string> parse_process(std::string_view line) {
    auto pos = line.find(":");
    if (std::string::npos == pos) {
      return {};
    }

    std::pair<std::string, std::string> result;

    std::string column = std::string(line.substr(0, pos));
    std::string value = ltrim(std::string(line.substr(pos + 1)));

    return std::make_pair(column, value);
  }

  Processes::Process read_process(std::string_view pid) {
    Processes::Process process;

    std::string root = "/proc/";
    std::ifstream stream(root + std::string(pid) + std::string("/status"));

    if (stream.is_open()) {
      std::string buffer;

      while (std::getline(stream, buffer)) {
        std::pair<std::string, std::string> line = parse_process(buffer);

        if ("Name" == line.first) {
          process.name = line.second;
        } else if ("Pid" == line.first) {
          process.pid = line.second;
        } else if ("PPid" == line.first) {
          process.ppid = line.second;
          break;
        }
      }

      stream.close();
    }

    return process;
  }

  wnd::utils::ProcessTree::Process convert(const Process& process) {
    wnd::utils::ProcessTree::Process adapter;
    adapter.memory = process.memory;
    adapter.name = process.name;
    adapter.pid = process.pid;
    adapter.ppid = process.ppid;

    float p_cpu = static_cast<float>(process.cpu - process.cpuLast) /
                  (wnd::system::Cpu::get_cpu_total() - this->cpuLast);

    adapter.p_cpu = p_cpu > 0 ? p_cpu * 100 : 0.f;

    return adapter;
  }

  void find_childs_for_process(std::string_view pid, std::vector<Processes::Process>& processes,
                               std::vector<Processes::Process>&& oldProcesses,
                               std::vector<wnd::utils::ProcessTree::Process>& ref) {
    for (auto& process : processes) {
      if (process.ppid == pid) {
        std::vector<wnd::utils::ProcessTree::Process> child;

        find_childs_for_process(process.pid, processes, std::move(oldProcesses), child);
        process.memory = wnd::ProcessMemory::get_memory_for_process(process.pid);
        process.cpu = wnd::ProcessCpu::get_cpu_for_process(process.pid);

        auto oldState =
            std::find_if(oldProcesses.cbegin(), oldProcesses.cend(),
                         [&process](const auto& old) { return old.pid == process.pid; });

        if (oldState != oldProcesses.cend()) {
          process.cpuLast = oldState->cpu;
        }

        wnd::utils::ProcessTree::Process adapter;
        adapter = this->convert(std::move(process));
        adapter.child = std::move(child);

        ref.push_back(adapter);
      }
    }
  }

  std::vector<Processes::Process> get_all_processes() {
    std::vector<Processes::Process> processes;

    std::string root = "/proc/";

    std::filesystem::path p(root);

    for (const auto& pathIterator : std::filesystem::directory_iterator(p)) {
      if (pathIterator.is_symlink()) continue;

      std::string id = pathIterator.path().filename();
      Processes::Process process = read_process(id);
      processes.push_back(std::move(process));
    }

    return processes;
  }

 public:
  wnd::utils::ProcessTree::Process get_tree_for_process(std::string_view pid) {
    std::vector<Processes::Process> processes = get_all_processes();

    wnd::utils::ProcessTree::Process adapter;

    auto iter = std::find_if(processes.begin(), processes.end(),
                             [&pid](const auto& process) { return process.pid == pid; });
    if (processes.end() == iter) return adapter;

    iter->memory = wnd::ProcessMemory::get_memory_for_process(pid);
    iter->cpu = wnd::ProcessCpu::get_cpu_for_process(pid);

    auto oldState = std::find_if(this->_processes.cbegin(), this->_processes.cend(),
                                 [&iter](const auto& old) { return old.pid == iter->pid; });

    if (oldState != this->_processes.cend()) {
      iter->cpuLast = oldState->cpu;
    }

    adapter = this->convert(*iter);

    this->find_childs_for_process(adapter.pid, processes, std::move(this->_processes),
                                  adapter.child);

    this->_processes = std::move(processes);
    this->cpuLast = wnd::system::Cpu::get_cpu_total();
    return adapter;
  }
};
}  // namespace

static Processes Process = Processes();

namespace wnd::utils {
ProcessTree::Process ProcessTree::get_tree_for_process(int pid) {
  if(pid <= 0) return;
  return ::Process.get_tree_for_process(std::to_string(pid));
}
};  // namespace wnd::utils

#endif
