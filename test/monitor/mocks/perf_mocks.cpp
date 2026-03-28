#include "mocks/perf_mocks.hpp"

#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>

PerfMocks& PerfMocks::instance() {
  static PerfMocks inst;
  return inst;
}

void PerfMocks::set_syscall_handler(SyscallHandler handler) {
  syscall_handler_ = handler;
}

void PerfMocks::set_ioctl_handler(IoctlHandler handler) {
  ioctl_handler_ = handler;
}

PerfMocks::SyscallHandler PerfMocks::get_syscall_handler() const {
  return syscall_handler_;
}

PerfMocks::IoctlHandler PerfMocks::get_ioctl_handler() const {
  return ioctl_handler_;
}

void PerfMocks::reset() {
  syscall_handler_ = nullptr;
  ioctl_handler_ = nullptr;
}

/**
 * @brief Default syscall handler - delegates to real syscall(SYS_perf_event_open, ...).
 */
static int default_syscall_handler(void* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

/**
 * @brief Default ioctl handler - delegates to real ioctl.
 */
static int default_ioctl_handler(int fd, unsigned long request, void* arg) {
  return ::ioctl(fd, request, arg);
}

/**
 * @brief Linker-wrapped syscall implementation.
 *
 * Called via linker wrap mechanism for perf_event_open syscalls.
 * Dispatches to registered handler or default.
 */
extern "C" int __wrap_syscall(long nr, void* arg1, pid_t arg2, int arg3, int arg4, unsigned long arg5) {
  if (nr == __NR_perf_event_open) {
    auto handler = PerfMocks::instance().get_syscall_handler();
    if (handler) {
      return handler(arg1, arg2, arg3, arg4, arg5);
    }
    return default_syscall_handler(arg1, arg2, arg3, arg4, arg5);
  }
  // For other syscalls, delegate to real syscall
  return syscall(nr, arg1, arg2, arg3, arg4, arg5);
}

/**
 * @brief Linker-wrapped ioctl implementation.
 *
 * Called via linker wrap mechanism. Dispatches to registered handler or default.
 */
extern "C" int __wrap_ioctl(int fd, unsigned long request, void* arg) {
  auto handler = PerfMocks::instance().get_ioctl_handler();
  if (handler) {
    return handler(fd, request, arg);
  }
  return default_ioctl_handler(fd, request, arg);
}
