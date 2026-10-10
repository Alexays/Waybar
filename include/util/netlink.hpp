#pragma once

#include <netlink/handlers.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>

namespace waybar::util {
// Like nl_send_sync(), but keeps reading until the kernel's ACK. nl_send_sync() stops after the
// first datagram, so a non-dump request whose reply and ACK arrive separately leaves the ACK
// queued on the socket. Takes ownership of msg. Returns 0 or a negative libnl error code.
int nl_send_and_wait_for_ack(struct nl_sock* sock, struct nl_msg* msg);
}  // namespace waybar::util
