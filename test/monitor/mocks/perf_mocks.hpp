#ifndef PERF_MOCKS_HPP
#define PERF_MOCKS_HPP

#include <sys/ioctl.h>
#include <sys/types.h>

#include <cstdint>
#include <functional>

/**
 * @brief Mock manager for perf syscalls and ioctl operations.
 *
 * This allows tests to control the behavior of:
 * - syscall(__NR_perf_event_open, ...)
 * - ioctl(fd, request, arg)
 */
class PerfMocks {
 public:
  // Signature: int perf_event_open(perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags)
  using SyscallHandler = std::function<int(void*, pid_t, int, int, unsigned long)>;
  using IoctlHandler = std::function<int(int, unsigned long, void*)>;

  static PerfMocks& instance();

  /**
   * @brief Set custom handler for perf_event_open syscall.
   *
   * Handler receives: (attr_ptr, pid, cpu, group_fd, flags)
   * and returns file descriptor or -1 on error.
   */
  void set_syscall_handler(SyscallHandler handler);

  /**
   * @brief Set custom handler for ioctl operations.
   *
   * Handler receives: (fd, request, arg) and returns result code.
   */
  void set_ioctl_handler(IoctlHandler handler);

  /**
   * @brief Get the registered syscall handler.
   */
  SyscallHandler get_syscall_handler() const;

  /**
   * @brief Get the registered ioctl handler.
   */
  IoctlHandler get_ioctl_handler() const;

  /**
   * @brief Reset all handlers to default (real syscalls/ioctl).
   */
  void reset();

 private:
  PerfMocks() = default;

  SyscallHandler syscall_handler_;
  IoctlHandler ioctl_handler_;
};

#endif
