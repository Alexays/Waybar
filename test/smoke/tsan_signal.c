// Preloaded into the ThreadSanitizer run. glibc's signal() installs the handler
// with SA_RESTART; TSan's interceptor drops it, so Waybar's blocking reads fail
// with EINTR there and nowhere else. Give signal() its glibc semantics back.
#define _GNU_SOURCE
#include <signal.h>

sighandler_t signal(int sig, sighandler_t handler) {
    struct sigaction act = {.sa_handler = handler, .sa_flags = SA_RESTART};
    struct sigaction old;
    sigemptyset(&act.sa_mask);
    return sigaction(sig, &act, &old) == 0 ? old.sa_handler : SIG_ERR;
}
