#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "giomm/file.h"
#include "giomm/filemonitor.h"
#include "glibmm/refptr.h"

struct pollfd;

namespace waybar {
class CssReloadHelper {
 public:
  CssReloadHelper(std::string cssFile, std::function<void()> callback);

  virtual ~CssReloadHelper() = default;

  virtual void monitorChanges();

 protected:
  std::vector<std::string> parseImports(const std::string& cssFile);

  void parseImports(const std::string& cssFile, std::unordered_map<std::string, bool>& imports);

  void watchFiles(const std::vector<std::string>& files);

  bool handleInotifyEvents(int fd);

  bool watch(int inotifyFd, pollfd* pollFd);

  virtual std::string getFileContents(const std::string& filename);

  virtual std::string findPath(const std::string& filename);

  void handleFileChange(Glib::RefPtr<Gio::File> const& file,
                        Glib::RefPtr<Gio::File> const& other_type,
                        Gio::FileMonitorEvent event_type);

 private:
  std::string m_cssFile;

  std::function<void()> m_callback;

  std::vector<std::tuple<Glib::RefPtr<Gio::FileMonitor>>> m_fileMonitors;
};
}  // namespace waybar
