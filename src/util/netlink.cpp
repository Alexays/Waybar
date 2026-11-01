#include "util/netlink.hpp"

namespace waybar::util {
int nl_send_and_wait_for_ack(struct nl_sock* sock, struct nl_msg* msg) {
  int err = nl_send_auto(sock, msg);
  nlmsg_free(msg);
  if (err < 0) {
    return err;
  }

  bool acked = false;
  struct nl_cb* sock_cb = nl_socket_get_cb(sock);
  struct nl_cb* cb = nl_cb_clone(sock_cb);
  nl_cb_put(sock_cb);
  if (cb == nullptr) {
    return -NLE_NOMEM;
  }
  nl_cb_set(
      cb, NL_CB_ACK, NL_CB_CUSTOM,
      [](struct nl_msg*, void* arg) {
        *static_cast<bool*>(arg) = true;
        return static_cast<int>(NL_STOP);
      },
      &acked);

  err = 0;
  while (!acked && err >= 0) {
    err = nl_recvmsgs(sock, cb);
  }
  nl_cb_put(cb);
  return err < 0 ? err : 0;
}
}  // namespace waybar::util
