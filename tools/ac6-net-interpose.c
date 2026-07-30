/*
 * AC6 guest-socket interposer.
 *
 * The thread-state census established that AC6's main guest thread parks
 * forever in recvfrom(fd, buf, 1281, 0, &from, &fromlen) at 0% CPU, and the
 * SDK holds exactly one recvfrom -- XSocket::RecvFrom, reachable only from the
 * guest export NetDll_recvfrom. Two questions decide the fix, and neither can
 * be answered from the source:
 *
 *   1. what did the guest do to that socket before receiving on it, and in
 *      particular did it ask for non-blocking? On the 360 that is
 *      ioctlsocket(s, FIONBIO, &1), where FIONBIO is the Winsock value
 *      0x8004667E. rex::net::socket_ioctl passes the guest's command number
 *      straight to the Linux ioctl(), whose FIONBIO is 0x5421, so such a
 *      request cannot take effect.
 *
 *   2. if the socket were non-blocking, would the guest get past this point?
 *
 * strace answers neither: its overhead on this process is large enough that
 * the guest never reaches the call. Rebuilding the runtime is the documented
 * route but costs an SDK build; this interposer costs a second and can also
 * *test* the fix before any code is changed.
 *
 * Safety: the runtime holds many AF_UNIX and AF_NETLINK sockets -- X11, dbus,
 * PipeWire, NSS -- and breaking those would break the process. Every hook here
 * therefore first asks the kernel for the socket's domain via SO_DOMAIN and
 * ignores anything that is not AF_INET/AF_INET6. Guest sockets are the only
 * AF_INET ones this process creates.
 *
 * Modes, by environment variable:
 *   AC6_NET_LOG=1        log AF_INET socket calls (default on)
 *   AC6_NET_BT=1         dump a host backtrace at the first blocking recvfrom
 *   AC6_NET_NONBLOCK=1   force AF_INET sockets non-blocking -- the experiment
 *   AC6_NET_LOGFILE=path where to write (default stderr)
 *
 * Build: gcc -shared -fPIC -O1 -o ac6-net-interpose.so ac6-net-interpose.c -ldl
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

static int (*real_socket)(int, int, int);
static ssize_t (*real_recvfrom)(int, void *, size_t, int, struct sockaddr *, socklen_t *);
static ssize_t (*real_recv)(int, void *, size_t, int);
static int (*real_ioctl)(int, unsigned long, void *);
static int (*real_bind)(int, const struct sockaddr *, socklen_t);
static int (*real_setsockopt)(int, int, int, const void *, socklen_t);
static ssize_t (*real_sendto)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);

static FILE *g_log;
static int g_do_log = 1, g_do_bt = 0, g_nonblock = 0;
static int g_bt_done = 0;
/* Guest ports below 1024 cannot be bound by an unprivileged Linux process
   (ip_unprivileged_port_start), while the Xbox 360 has no such rule. Shifting
   them by a fixed offset makes the bind succeed without any privilege, which
   matters because the deliverable must run as an ordinary user. */
static unsigned g_port_offset = 0;

static unsigned short shift_port(unsigned short host_order) {
  if (g_port_offset && host_order != 0 && host_order < 1024) {
    return (unsigned short)(host_order + g_port_offset);
  }
  return host_order;
}

/* The Winsock FIONBIO the guest would use; Linux's is 0x5421. */
#define WINSOCK_FIONBIO 0x8004667EUL
#define WINSOCK_FIONREAD 0x4004667FUL

static void init(void) {
  static int done = 0;
  if (done) return;
  done = 1;
  real_socket = dlsym(RTLD_NEXT, "socket");
  real_recvfrom = dlsym(RTLD_NEXT, "recvfrom");
  real_recv = dlsym(RTLD_NEXT, "recv");
  real_ioctl = dlsym(RTLD_NEXT, "ioctl");
  real_bind = dlsym(RTLD_NEXT, "bind");
  real_setsockopt = dlsym(RTLD_NEXT, "setsockopt");

  const char *p = getenv("AC6_NET_LOGFILE");
  g_log = (p && *p) ? fopen(p, "a") : NULL;
  if (!g_log) g_log = stderr;
  setvbuf(g_log, NULL, _IOLBF, 0);

  p = getenv("AC6_NET_LOG");
  g_do_log = !p || *p != '0';
  p = getenv("AC6_NET_BT");
  g_do_bt = p && *p == '1';
  p = getenv("AC6_NET_NONBLOCK");
  g_nonblock = p && *p == '1';
  p = getenv("AC6_NET_PORT_OFFSET");
  g_port_offset = (p && *p) ? (unsigned)strtoul(p, NULL, 0) : 0u;

  fprintf(g_log, "[ac6-net] interposer active: log=%d bt=%d nonblock=%d port_offset=%u\n", g_do_log,
          g_do_bt, g_nonblock, g_port_offset);
}

/* Only AF_INET/AF_INET6 descriptors are guest sockets in this process. */
static int is_inet(int fd) {
  int dom = 0;
  socklen_t len = sizeof(dom);
  if (!real_setsockopt) init();
  if (getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &dom, &len) != 0) return 0;
  return dom == AF_INET || dom == AF_INET6;
}

static void dump_bt(const char *why) {
  void *frames[48];
  int n = backtrace(frames, 48);
  fprintf(g_log, "[ac6-net] backtrace (%s), %d frames:\n", why, n);
  /* Print raw addresses too: the runtime is a 165 MB LTO binary and
     backtrace_symbols often yields only the module, so addr2line on these
     offsets is what actually resolves the guest export. */
  char **syms = backtrace_symbols(frames, n);
  for (int i = 0; i < n; i++) {
    fprintf(g_log, "    #%-2d %p  %s\n", i, frames[i], syms ? syms[i] : "");
  }
  free(syms);
}

int socket(int domain, int type, int protocol) {
  init();
  int fd = real_socket(domain, type, protocol);
  if (g_do_log && (domain == AF_INET || domain == AF_INET6)) {
    fprintf(g_log, "[ac6-net] socket(domain=%d type=%d proto=%d) = %d\n", domain, type, protocol,
            fd);
    if (g_nonblock && fd >= 0) {
      int fl = fcntl(fd, F_GETFL, 0);
      fcntl(fd, F_SETFL, fl | O_NONBLOCK);
      fprintf(g_log, "[ac6-net]   -> forced O_NONBLOCK on fd %d\n", fd);
    }
  }
  return fd;
}

int bind(int fd, const struct sockaddr *addr, socklen_t len) {
  init();
  int inet = is_inet(fd);
  unsigned want = 0, used = 0;

  if (inet && addr && addr->sa_family == AF_INET && len >= (socklen_t)sizeof(struct sockaddr_in)) {
    struct sockaddr_in fixed = *(const struct sockaddr_in *)addr;
    want = (unsigned)ntohs(fixed.sin_port);
    used = shift_port((unsigned short)want);
    if (used != want) {
      fixed.sin_port = htons((unsigned short)used);
      int r = real_bind(fd, (const struct sockaddr *)&fixed, len);
      fprintf(g_log,
              "[ac6-net] bind(fd=%d guest_port=%u) remapped to host_port=%u = %d%s\n", fd, want,
              used, r, r < 0 ? " ERR" : "");
      return r;
    }
  }

  int r = real_bind(fd, addr, len);
  if (g_do_log && inet) {
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    fprintf(g_log, "[ac6-net] bind(fd=%d port=%u) = %d%s\n", fd, (unsigned)ntohs(in->sin_port), r,
            (r < 0 && errno == EACCES) ? " EACCES (privileged port)" : "");
  }
  return r;
}

int setsockopt(int fd, int level, int optname, const void *val, socklen_t len) {
  init();
  int r = real_setsockopt(fd, level, optname, val, len);
  if (g_do_log && is_inet(fd)) {
    unsigned v = (val && len >= 4) ? *(const unsigned *)val : 0;
    fprintf(g_log, "[ac6-net] setsockopt(fd=%d level=%d opt=%d val=%u len=%u) = %d\n", fd, level,
            optname, v, (unsigned)len, r);
  }
  return r;
}

int ioctl(int fd, unsigned long request, ...) {
  init();
  va_list ap;
  va_start(ap, request);
  void *arg = va_arg(ap, void *);
  va_end(ap);

  /* This is the crux: a guest FIONBIO arrives here with the Winsock command
     number, which Linux does not recognise, so the socket silently stays
     blocking. Log it, and when running the experiment, honour it. */
  if (is_inet(fd) && (request == WINSOCK_FIONBIO || request == WINSOCK_FIONREAD)) {
    unsigned v = arg ? *(unsigned *)arg : 0;
    fprintf(g_log, "[ac6-net] ioctl(fd=%d, WINSOCK %s, arg=%u) -- Linux would reject this\n", fd,
            request == WINSOCK_FIONBIO ? "FIONBIO" : "FIONREAD", v);
    if (request == WINSOCK_FIONBIO) {
      int fl = fcntl(fd, F_GETFL, 0);
      if (v) {
        fcntl(fd, F_SETFL, fl | O_NONBLOCK);
      } else {
        fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
      }
      fprintf(g_log, "[ac6-net]   -> translated to O_NONBLOCK=%u on fd %d\n", v, fd);
      return 0;
    }
  }
  if (g_do_log && is_inet(fd)) {
    fprintf(g_log, "[ac6-net] ioctl(fd=%d, request=0x%lx)\n", fd, request);
  }
  return real_ioctl(fd, request, arg);
}

ssize_t recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *from,
                 socklen_t *fromlen) {
  init();
  if (is_inet(fd)) {
    if (g_do_log) {
      fprintf(g_log, "[ac6-net] recvfrom(fd=%d len=%zu flags=%d) entering\n", fd, len, flags);
    }
    if (g_do_bt && !g_bt_done) {
      g_bt_done = 1;
      dump_bt("first guest recvfrom");
    }
    if (g_nonblock) {
      int fl = fcntl(fd, F_GETFL, 0);
      if (!(fl & O_NONBLOCK)) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    }
    ssize_t r = real_recvfrom(fd, buf, len, flags, from, fromlen);
    if (g_do_log) {
      fprintf(g_log, "[ac6-net] recvfrom(fd=%d) = %zd%s\n", fd, r,
              r < 0 ? (errno == EAGAIN ? " EAGAIN" : " ERR") : "");
    }
    return r;
  }
  return real_recvfrom(fd, buf, len, flags, from, fromlen);
}

/* The same shift has to apply to outbound traffic, or a guest that talks to
   itself on loopback would send to the unshifted port and never be heard. */
ssize_t sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *to,
               socklen_t tolen) {
  init();
  if (!real_sendto) real_sendto = dlsym(RTLD_NEXT, "sendto");
  if (is_inet(fd) && to && to->sa_family == AF_INET &&
      tolen >= (socklen_t)sizeof(struct sockaddr_in)) {
    struct sockaddr_in fixed = *(const struct sockaddr_in *)to;
    unsigned want = (unsigned)ntohs(fixed.sin_port);
    unsigned used = shift_port((unsigned short)want);
    if (g_do_log) {
      fprintf(g_log, "[ac6-net] sendto(fd=%d len=%zu guest_port=%u host_port=%u)\n", fd, len, want,
              used);
    }
    if (used != want) {
      fixed.sin_port = htons((unsigned short)used);
      return real_sendto(fd, buf, len, flags, (const struct sockaddr *)&fixed, tolen);
    }
  }
  return real_sendto(fd, buf, len, flags, to, tolen);
}

ssize_t recv(int fd, void *buf, size_t len, int flags) {
  init();
  if (is_inet(fd)) {
    if (g_do_log) fprintf(g_log, "[ac6-net] recv(fd=%d len=%zu) entering\n", fd, len);
    if (g_nonblock) {
      int fl = fcntl(fd, F_GETFL, 0);
      if (!(fl & O_NONBLOCK)) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    }
    ssize_t r = real_recv(fd, buf, len, flags);
    if (g_do_log) fprintf(g_log, "[ac6-net] recv(fd=%d) = %zd\n", fd, r);
    return r;
  }
  return real_recv(fd, buf, len, flags);
}
