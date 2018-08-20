#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc.hpp"

namespace waybar::modules::sway {

class Ipc {
  public:
    Ipc();
    ~Ipc();

		struct ipc_response {
			uint32_t size;
			uint32_t type;
			std::string payload;
		};

		void connect();
		struct ipc_response sendCmd(uint32_t type,
			const std::string& payload = "") const;
		void subscribe(const std::string& payload) const;
		struct ipc_response handleEvent() const;
  protected:
		static inline const std::string ipc_magic_ = "i3-ipc";
		static inline const size_t ipc_header_size_ = ipc_magic_.size() + 8;

		const std::string getSocketPath() const;
		int open(const std::string&) const;
		struct ipc_response send(int fd, uint32_t type,
			const std::string& payload = "") const;
		struct ipc_response recv(int fd) const;

		int fd_;
		int fd_event_;
};

}
