#include <unistd.h>

#include <list>
#include <mutex>

std::mutex reapMtx;
std::list<pid_t> reap;
