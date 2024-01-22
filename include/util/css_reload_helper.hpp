#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct pollfd;

namespace waybar {
class CssReloadHelper {
 public:
  CssReloadHelper(std::string cssFile, std::function<void()> callback);

  ~CssReloadHelper();

  virtual void monitorChanges();

  void stop();

 protected:
  std::vector<std::string> parseImports(const std::string& cssFile);

  void parseImports(const std::string& cssFile,
                           std::unordered_map<std::string, bool>& imports);


  void watchFiles(const std::vector<std::string>& files);

  bool handleInotifyEvents(int fd);

  bool watch(int inotifyFd, pollfd * pollFd);

  virtual std::string getFileContents(const std::string& filename);

  virtual std::string findPath(const std::string& filename);

 private:
  std::string m_cssFile;
  std::function<void()> m_callback;
  std::atomic<bool> m_running = false;
  std::thread m_thread;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};
}  // namespace waybar
