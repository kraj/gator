/* Copyright (C) 2018-2022 by Arm Limited. All rights reserved. */

#include "Syscall.h"

#include <csignal>
#include <cstdlib>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

namespace lib {
    int close(int fd) { return ::close(fd); }
    int open(const char * path, int flag) { return ::open(path, flag); }
    int open(const char * path, int flag, mode_t mode) { return ::open(path, flag, mode); };
    int fcntl(int fd, int cmd, unsigned long arg) { return ::fcntl(fd, cmd, arg); }

    int ioctl(int fd, unsigned long int request, unsigned long arg) { return ::ioctl(fd, request, arg); }

    void * mmap(void * addr, size_t length, int prot, int flags, int fd, off_t offset)
    {
        return ::mmap(addr, length, prot, flags, fd, offset);
    }

    int munmap(void * addr, size_t length) { return ::munmap(addr, length); }

    int perf_event_open(struct perf_event_attr * const attr,
                        const pid_t pid,
                        const int cpu,
                        const int group_fd,
                        const unsigned long flags)
    {
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
    }

    int accept4(int sockfd, struct sockaddr * addr, socklen_t * addrlen, int flags)
    {
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        return syscall(__NR_accept4, sockfd, addr, addrlen, flags);
    }

    ssize_t read(int fd, void * buf, size_t count) { return ::read(fd, buf, count); }

    ssize_t write(int fd, const void * buf, size_t count) { return ::write(fd, buf, count); }

    int pipe2(std::array<int, 2> & fds, int flags) { return ::pipe2(fds.data(), flags); }

    int uname(struct utsname * buf) { return ::uname(buf); }

    uid_t geteuid() { return ::geteuid(); }

    pid_t waitpid(pid_t pid, int * wstatus, int options) { return ::waitpid(pid, wstatus, options); }
    int poll(struct pollfd * fds, nfds_t nfds, int timeout) { return ::poll(fds, nfds, timeout); }

    int access(const char * filename, int how) { return ::access(filename, how); }

    void exit(int status)
    {
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        ::exit(status);
    }

    int kill(pid_t pid, int signal) { return ::kill(pid, signal); }

    pid_t getppid() { return ::getppid(); }
    pid_t getpid() { return ::getpid(); }
    pid_t gettid() { return pid_t(syscall(__NR_gettid)); }
}
