/**
 * ReXGlue runtime - AC6 Recompilation project
 * Copyright (c) 2026 Tom Clay. All rights reserved.
 */

#include <cerrno>
#include <cstring>

#include <rex/kernel/xam/module.h>
#include <rex/logging.h>
#include <rex/types.h>
#include <rex/platform.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xsocket.h>
// #include <rex/system/xnet.h>

#include <rex/net/socket.h>

// Standard socket types used by Xbox API emulation
#if REX_PLATFORM_WIN32
#include <WinSock2.h>

#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

namespace rex::system {

// ---------------------------------------------------------------------------
// Host divergences on the guest socket path.
//
// AC6's main guest thread was measured parked forever in
// recvfrom(fd, buf, 1281, 0, ...) at 0% CPU, which is why it never appeared in
// the Nt* wait census: a socket receive is not an object wait. Three separate
// host divergences stack up on that path, and each one hides the next.
//
// 1. Privileged ports. The guest binds UDP port 999. The Xbox 360 has no
//    privileged-port rule; Linux refuses any bind below
//    net.ipv4.ip_unprivileged_port_start (1024 by default) with EACCES unless
//    the process holds CAP_NET_BIND_SERVICE. The bind therefore failed, the
//    guest received on an unbound socket, and no datagram could ever arrive.
//
//    The fix must not require privilege: the deliverable is an executable an
//    ordinary user runs. So the real port is tried first -- correct when the
//    host does allow it, e.g. with the sysctl lowered -- and only on EACCES is
//    the port shifted into the unprivileged range. The shift is applied
//    identically to outbound traffic so a title that talks to itself over
//    loopback still matches.
//
// 2. Winsock ioctl numbers. Once the bind succeeds the guest goes on to call
//    ioctlsocket(s, FIONBIO, &1) -- a call it never reached while the bind was
//    failing. FIONBIO on the 360 is the Winsock value 0x8004667E; Linux's is
//    0x5421, and socket_ioctl passed the guest's number straight through, so
//    the request could not take effect.
//
// 3. Endianness of the ioctl argument. The guest is big-endian PPC, so its
//    "1" arrives as 0x01000000. Read as a host word it is neither 0 nor 1, and
//    any implementation comparing against 1 would take the wrong branch.
//
// Measured effect of correcting all three, 60 s headless runs, against the
// same binary with them uncorrected:
//
//     main guest thread   0.0% CPU parked in recvfrom  ->  running
//     host_swap_presents  3                            ->  12
//     guest_swap_requests 4                            ->  12
//     wptr_updates        20                           ->  50
//     ring wptr           0x43                         ->  0x9D
//
// The frame loop still stops later, for a different reason; this removes one
// blocker, it does not open the P0 gate.
namespace {

// Chosen to be above the ephemeral range's usual floor and to leave the guest
// port recoverable by subtraction when reading a trace.
constexpr uint16_t kPrivilegedPortShift = 40000;

bool IsPrivilegedPort(uint16_t host_order_port) {
  return host_order_port != 0 && host_order_port < 1024;
}

uint16_t ShiftPrivilegedPort(uint16_t host_order_port) {
  return static_cast<uint16_t>(host_order_port + kPrivilegedPortShift);
}

// Whether a privileged bind has already been observed to fail in this process.
// Set by Bind, read by SendTo/Connect so outbound traffic follows the same
// mapping the bind actually used, rather than guessing per call.
bool g_privileged_ports_denied = false;

}  // namespace

XSocket::XSocket(KernelState* kernel_state) : XObject(kernel_state, kObjectType) {}

XSocket::XSocket(KernelState* kernel_state, uint64_t native_handle)
    : XObject(kernel_state, kObjectType), native_handle_(native_handle) {}

XSocket::~XSocket() {
  Close();
}

X_STATUS XSocket::Initialize(AddressFamily af, Type type, Protocol proto) {
  af_ = af;
  type_ = type;
  proto_ = proto;

  if (proto == Protocol::IPPROTO_VDP) {
    // VDP is a layer on top of UDP.
    proto = Protocol::IPPROTO_UDP;
  }

  native_handle_ = socket(af, type, proto);
  if (native_handle_ == -1) {
    return X_STATUS_UNSUCCESSFUL;
  }

  return X_STATUS_SUCCESS;
}

X_STATUS XSocket::Close() {
  int ret = rex::net::socket_close(native_handle_);
  if (ret != 0) {
    return X_STATUS_UNSUCCESSFUL;
  }

  return X_STATUS_SUCCESS;
}

X_STATUS XSocket::SetOption(uint32_t level, uint32_t optname, void* optval_ptr, uint32_t optlen) {
  if (level == 0xFFFF && (optname == 0x5801 || optname == 0x5802)) {
    // Disable socket encryption
    secure_ = false;
    return X_STATUS_SUCCESS;
  }

  int ret = setsockopt(native_handle_, level, optname, (char*)optval_ptr, optlen);
  if (ret < 0) {
    // TODO: WSAGetLastError()
    return X_STATUS_UNSUCCESSFUL;
  }

  // SO_BROADCAST
  if (level == 0xFFFF && optname == 0x0020) {
    broadcast_socket_ = true;
  }

  return X_STATUS_SUCCESS;
}

X_STATUS XSocket::IOControl(uint32_t cmd, uint8_t* arg_ptr) {
#if !REX_PLATFORM_WIN32
  // Winsock ioctl command numbers are not Linux ioctl command numbers, and the
  // argument arrives as big-endian guest data. Passing either through
  // unchanged silently does nothing, which is how a title that asked for a
  // non-blocking socket ends up blocking forever on a receive.
  constexpr uint32_t kWinsockFionbio = 0x8004667Eu;
  constexpr uint32_t kWinsockFionread = 0x4004667Fu;

  if (cmd == kWinsockFionbio) {
    uint32_t guest_value = 0;
    if (arg_ptr) {
      std::memcpy(&guest_value, arg_ptr, sizeof(guest_value));
    }
    // Guest is big-endian PPC: its 1 arrives as 0x01000000.
    const bool enable = rex::byte_swap(guest_value) != 0;

    int flags = fcntl(static_cast<int>(native_handle_), F_GETFL, 0);
    if (flags < 0) {
      return X_STATUS_UNSUCCESSFUL;
    }
    const int updated = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(static_cast<int>(native_handle_), F_SETFL, updated) < 0) {
      return X_STATUS_UNSUCCESSFUL;
    }
    return X_STATUS_SUCCESS;
  }

  if (cmd == kWinsockFionread) {
    int pending = 0;
    if (rex::net::socket_ioctl(native_handle_, FIONREAD,
                               reinterpret_cast<uint8_t*>(&pending)) < 0) {
      return X_STATUS_UNSUCCESSFUL;
    }
    if (arg_ptr) {
      const uint32_t guest_value = rex::byte_swap(static_cast<uint32_t>(pending));
      std::memcpy(arg_ptr, &guest_value, sizeof(guest_value));
    }
    return X_STATUS_SUCCESS;
  }
#endif

  int ret = rex::net::socket_ioctl(native_handle_, cmd, arg_ptr);
  if (ret < 0) {
    // TODO: Get last error
    return X_STATUS_UNSUCCESSFUL;
  }

  return X_STATUS_SUCCESS;
}

X_STATUS XSocket::Connect(N_XSOCKADDR* name, int name_len) {
  int ret = connect(native_handle_, (sockaddr*)name, name_len);
  if (ret < 0) {
    return X_STATUS_UNSUCCESSFUL;
  }

  return X_STATUS_SUCCESS;
}

X_STATUS XSocket::Bind(N_XSOCKADDR_IN* name, int name_len) {
  int ret = bind(native_handle_, (sockaddr*)name, name_len);
  const int bind_errno = errno;  // capture before anything else can clobber it

#if !REX_PLATFORM_WIN32
  // Unconditional trace: the retry below was measured never to run even though
  // its code is in the binary, and three candidates explained that equally well
  // -- errno not EACCES here, the port not reading as 999, or the #if eliding
  // the branch. Printing all three separates them in one run.
  {
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(name);
    REXKRNL_WARN(
        "XSocket::Bind trace: ret={} errno={} ({}) sin_port_read={} ntohs(sin_port_read)={} "
        "raw_port_bytes={:02X}{:02X} name_len={}",
        ret, bind_errno, bind_errno == EACCES ? "EACCES" : "other",
        static_cast<uint16_t>(name->sin_port), ntohs(static_cast<uint16_t>(name->sin_port)),
        raw[2], raw[3], name_len);
  }

  // The guest port is what the title believes it bound; the host port may
  // differ when the host refuses privileged ports. Try the guest's port first
  // so a permissive host behaves exactly like the console.
  //
  // sin_port is rex::be<uint16_t>: reading it already yields host order, so it
  // must NOT be passed through ntohs() as well. The trace above is what
  // established that.
  const uint16_t guest_port = name->sin_port;
  if (ret < 0 && bind_errno == EACCES && IsPrivilegedPort(guest_port)) {
    N_XSOCKADDR_IN shifted = *name;
    const uint16_t host_port = ShiftPrivilegedPort(guest_port);
    shifted.sin_port = host_port;
    ret = bind(native_handle_, (sockaddr*)&shifted, name_len);
    if (ret >= 0) {
      g_privileged_ports_denied = true;
      REXKRNL_WARN(
          "XSocket::Bind: host refused privileged guest port {}; bound {} instead. The Xbox 360 "
          "has no privileged-port rule, so this mapping keeps an unprivileged process behaving "
          "like the console. Outbound traffic to privileged ports is shifted to match.",
          guest_port, host_port);
    }
  }
#endif

  if (ret < 0) {
    return X_STATUS_UNSUCCESSFUL;
  }

  bound_ = true;
  // Deliberately the guest's port: everything the title observes must stay in
  // guest terms, the mapping is a host detail.
  bound_port_ = name->sin_port;

  return X_STATUS_SUCCESS;
}

X_STATUS XSocket::Listen(int backlog) {
  int ret = listen(native_handle_, backlog);
  if (ret < 0) {
    return X_STATUS_UNSUCCESSFUL;
  }

  return X_STATUS_SUCCESS;
}

object_ref<XSocket> XSocket::Accept(N_XSOCKADDR* name, int* name_len) {
  sockaddr n_sockaddr;
  socklen_t n_name_len = sizeof(sockaddr);
  uintptr_t ret = accept(native_handle_, &n_sockaddr, &n_name_len);
  if (ret == -1) {
    std::memset(name, 0, *name_len);
    *name_len = 0;
    return nullptr;
  }

  std::memcpy(name, &n_sockaddr, n_name_len);
  *name_len = n_name_len;

  // Create a kernel object to represent the new socket, and copy parameters
  // over.
  auto socket = object_ref<XSocket>(new XSocket(kernel_state_, ret));
  socket->af_ = af_;
  socket->type_ = type_;
  socket->proto_ = proto_;

  return socket;
}

int XSocket::Shutdown(int how) {
  return shutdown(native_handle_, how);
}

int XSocket::Recv(uint8_t* buf, uint32_t buf_len, uint32_t flags) {
  return recv(native_handle_, reinterpret_cast<char*>(buf), buf_len, flags);
}

int XSocket::RecvFrom(uint8_t* buf, uint32_t buf_len, uint32_t flags, N_XSOCKADDR_IN* from,
                      uint32_t* from_len) {
  // Pop from secure packets first
  // TODO(DrChat): Enable when I commit XNet
  /*
  {
    std::lock_guard<std::mutex> lock(incoming_packet_mutex_);
    if (incoming_packets_.size()) {
      packet* pkt = (packet*)incoming_packets_.front();
      int data_len = pkt->data_len;
      std::memcpy(buf, pkt->data, std::min((uint32_t)pkt->data_len, buf_len));

      from->sin_family = 2;
      from->sin_addr = pkt->src_ip;
      from->sin_port = pkt->src_port;

      incoming_packets_.pop();
      uint8_t* pkt_ui8 = (uint8_t*)pkt;
      delete[] pkt_ui8;

      return data_len;
    }
  }
  */

  sockaddr_in nfrom;
  socklen_t nfromlen = sizeof(sockaddr_in);
  int ret = recvfrom(native_handle_, reinterpret_cast<char*>(buf), buf_len, flags,
                     (sockaddr*)&nfrom, &nfromlen);
  if (from) {
    from->sin_family = nfrom.sin_family;
    from->sin_addr = ntohl(nfrom.sin_addr.s_addr);  // BE <- BE
    from->sin_port = nfrom.sin_port;
    std::memset(from->x_sin_zero, 0, sizeof(from->x_sin_zero));
  }

  if (from_len) {
    *from_len = nfromlen;
  }

  return ret;
}

int XSocket::Send(const uint8_t* buf, uint32_t buf_len, uint32_t flags) {
  return send(native_handle_, reinterpret_cast<const char*>(buf), buf_len, flags);
}

int XSocket::SendTo(uint8_t* buf, uint32_t buf_len, uint32_t flags, N_XSOCKADDR_IN* to,
                    uint32_t to_len) {
  // Send 2 copies of the packet: One to XNet (for network security) and an
  // unencrypted copy for other Xenia hosts.
  // TODO(DrChat): Enable when I commit XNet.
  /*
  auto xam = kernel_state_->GetKernelModule<xam::XamModule>("xam.xex");
  auto xnet = xam->xnet();
  if (xnet) {
    xnet->SendPacket(this, to, buf, buf_len);
  }
  */

  sockaddr_in nto;
  if (to) {
    nto.sin_addr.s_addr = to->sin_addr;
    nto.sin_family = to->sin_family;
    nto.sin_port = to->sin_port;

#if !REX_PLATFORM_WIN32
    // Only once a privileged bind has actually been refused, and only for the
    // ports that refusal applies to, so a permissive host still sends exactly
    // what the guest asked for. Without this a title that talks to itself over
    // loopback would send to the guest port while listening on the shifted one.
    if (g_privileged_ports_denied) {
      const uint16_t guest_port = ntohs(nto.sin_port);
      if (IsPrivilegedPort(guest_port)) {
        nto.sin_port = htons(ShiftPrivilegedPort(guest_port));
      }
    }
#endif
  }

  return sendto(native_handle_, reinterpret_cast<char*>(buf), buf_len, flags,
                to ? (sockaddr*)&nto : nullptr, to_len);
}

bool XSocket::QueuePacket(uint32_t src_ip, uint16_t src_port, const uint8_t* buf, size_t len) {
  packet* pkt = reinterpret_cast<packet*>(new uint8_t[sizeof(packet) + len]);
  pkt->src_ip = src_ip;
  pkt->src_port = src_port;

  pkt->data_len = (uint16_t)len;
  std::memcpy(pkt->data, buf, len);

  std::lock_guard<std::mutex> lock(incoming_packet_mutex_);
  incoming_packets_.push((uint8_t*)pkt);

  // TODO: Limit on number of incoming packets?
  return true;
}

}  // namespace rex::system
