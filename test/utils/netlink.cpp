#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <linux/genetlink.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <sys/socket.h>

#include "util/netlink.hpp"

namespace {
// CTRL_CMD_GETFAMILY without NLM_F_DUMP is answered with one reply and a separate ACK, the same
// shape as NL80211_CMD_GET_STATION, and works without wireless hardware or privileges.
struct nl_msg* get_family_request(const char* name) {
  struct nl_msg* msg = nlmsg_alloc();
  genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, GENL_ID_CTRL, 0, 0, CTRL_CMD_GETFAMILY, 1);
  nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, name);
  return msg;
}

bool socket_queue_empty(struct nl_sock* sock) {
  char buf[64];
  return recv(nl_socket_get_fd(sock), buf, sizeof(buf), MSG_DONTWAIT | MSG_PEEK) < 0;
}
}  // namespace

TEST_CASE("nl_send_and_wait_for_ack consumes reply and ACK", "[util][netlink]") {
  struct nl_sock* sock = nl_socket_alloc();
  REQUIRE(sock != nullptr);
  REQUIRE(genl_connect(sock) == 0);

  int replies = 0;
  nl_socket_modify_cb(
      sock, NL_CB_VALID, NL_CB_CUSTOM,
      [](struct nl_msg*, void* arg) {
        ++*static_cast<int*>(arg);
        return static_cast<int>(NL_OK);
      },
      &replies);

  SECTION("repeated requests do not accumulate on the socket") {
    for (int i = 0; i < 100; ++i) {
      REQUIRE(waybar::util::nl_send_and_wait_for_ack(sock, get_family_request("nlctrl")) == 0);
    }
    CHECK(replies == 100);
    CHECK(socket_queue_empty(sock));
  }

  SECTION("a kernel error is returned and leaves the socket usable") {
    CHECK(waybar::util::nl_send_and_wait_for_ack(sock, get_family_request("no-such-family")) < 0);
    CHECK(socket_queue_empty(sock));
    CHECK(waybar::util::nl_send_and_wait_for_ack(sock, get_family_request("nlctrl")) == 0);
    CHECK(replies == 1);
  }

  nl_close(sock);
  nl_socket_free(sock);
}
