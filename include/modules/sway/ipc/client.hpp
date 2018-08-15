#pragma once

#include <iostream>
#include "ipc.hpp"

/**
 * IPC response including type of IPC response, size of payload and the json
 * encoded payload string.
 */
struct ipc_response {
	uint32_t size;
	uint32_t type;
	std::string payload;
};

/**
 * Gets the path to the IPC socket from sway.
 */
std::string get_socketpath(void);
/**
 * Opens the sway socket.
 */
int ipc_open_socket(std::string socket_path);
/**
 * Issues a single IPC command and returns the buffer. len will be updated with
 * the length of the buffer returned from sway.
 */
std::string ipc_single_command(int socketfd, uint32_t type, const char *payload, uint32_t *len);
/**
 * Receives a single IPC response and returns an ipc_response.
 */
struct ipc_response ipc_recv_response(int socketfd);
