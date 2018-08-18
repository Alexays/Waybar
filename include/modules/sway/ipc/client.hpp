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
std::string getSocketPath(void);
/**
 * Opens the sway socket.
 */
int ipcOpenSocket(const std::string &socketPath);
/**
 * Issues a single IPC command and returns the buffer. len will be updated with
 * the length of the buffer returned from sway.
 */
struct ipc_response ipcSingleCommand(int socketfd, uint32_t type,
  const std::string& payload);
/**
 * Receives a single IPC response and returns an ipc_response.
 */
struct ipc_response ipcRecvResponse(int socketfd);
