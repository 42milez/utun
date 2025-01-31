#include <iostream>
#include <sys/kern_control.h>
#include <netinet/in.h>
#include <net/if_utun.h>
#include <sys/ioctl.h>
#include <sys/kern_event.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int open_utun(u_int32_t unit) {

  // Protocol families are mapped onto address families
  // --------------------------------------------------
  // Notes:
  //  - KEXT Controls and Notifications
  //    https://developer.apple.com/library/content/documentation/Darwin/Conceptual/NKEConceptual/control/control.html
  auto fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
  if (fd == -1) {
    std::cerr << "socket(SYSPROTO_CONTROL)" << std::endl;
    return -1;
  }

  // set the socket non-blocking
  auto err = fcntl(fd, F_SETFL, O_NONBLOCK);
  if (err != 0) {
    close(fd);
    return err;
  }

  // set close-on-exec flag
  auto flags = fcntl(fd, F_GETFD);
  flags |= FD_CLOEXEC;
  err = fcntl(fd, F_SETFD, flags);
  if (err != 0) {
    close(fd);
    return err;
  }

  // Convert a kernel control name to a kernel control id
  // --------------------------------------------------
  // Notes:
  //  - CTLIOCGINFO
  //    https://developer.apple.com/documentation/kernel/ctliocginfo?language=objc
  //  - strlcpy
  //    https://en.wikipedia.org/wiki/C_string_handling
  ctl_info ci{};

  if (strlcpy(ci.ctl_name, UTUN_CONTROL_NAME, sizeof(ci.ctl_name)) >= sizeof(ci.ctl_name)) {
    std::cerr << "ERROR: UTUN_CONTROL_NAME is too long" << std::endl;
    return -1;
  }

  if (ioctl(fd, CTLIOCGINFO, &ci) == -1) {
    std::cerr << "ERROR: ioctl(CTLIOCGINFO)" << std::endl;
    close(fd);
    return -1;
  }

  sockaddr_ctl sc{};

  sc.sc_id      = ci.ctl_id;
  sc.sc_len     = sizeof(sc);
  sc.sc_family  = AF_SYSTEM;
  sc.ss_sysaddr = AF_SYS_CONTROL;
  sc.sc_unit    = unit;

  if (connect(fd, (struct sockaddr *)&sc, sizeof(sc)) == -1) {
    std::cerr << "ERROR: connect(AF_SYS_CONTROL)" << std::endl;
    close(fd);
    return -1;
  }

  return fd;
}

void printSome(unsigned char c[], int len, char *pipe) {
  for (int i = 0; i < len; i++) {
    if (i % 0x10 == 0) {
      fprintf(stderr, "\n%s %04x ", pipe, i);
    }
    if (i % 8 == 0) {
      fprintf(stderr, " ");
    }
    fprintf(stderr, "%02x ", c[i]);
  }
  fprintf(stderr, "\n\n");
}

int main() {
  auto sock = open_utun(static_cast<u_int32_t >(strtol("10.0.7.1", nullptr, 10) + 1));
  if (sock >= 0) {
    printf("INFO: A tun interface has been created.\n");
  }

  system("ifconfig utun10 inet 10.0.7.1 10.0.7.1 mtu 1500 up");

  // http://www.geekpage.jp/programming/macosX-network/select-with-timeout.php
  fd_set fds{}, readfds{};
  char buf[2048];
  int maxfd = sock;
  timeval tv{};

  FD_ZERO(&readfds);
  FD_SET(sock, &readfds);

  tv.tv_sec = 10;
  tv.tv_usec = 0;

  while (1) {
    memcpy(&fds, &readfds, sizeof(fd_set));
    if (select(maxfd + 1, &fds, nullptr, nullptr, &tv) == 0) {
      printf("timeout\n");
      break;
    }
    if (FD_ISSET(sock, &fds)) {
      memset(buf, 0, sizeof(buf));
      ssize_t n = read(sock, buf, sizeof(buf));
      if (n < 0) {
        std::cerr << "read()" << std::endl;
        close(sock);
        return 1;
      }
      if (n == 0) {
        break;
      }
      auto ip_header = reinterpret_cast<ip*>(buf);
      std::cout << "size of ip header:  " << sizeof(ip) << std::endl;
      std::cout << "ip_tos:             " << htons(ip_header->ip_tos) << std::endl;
      std::cout << "ip_len:             " << htons(ip_header->ip_len) << std::endl;
      std::cout << "ip_id:              " << htons(ip_header->ip_id) << std::endl;
      std::cout << "ip_off:             " << htons(ip_header->ip_off) << std::endl;
      std::cout << "ip_ttl:             " << htons(ip_header->ip_ttl) << std::endl;
      std::cout << "ip_p:               " << htons(ip_header->ip_p) << std::endl;
      std::cout << "ip_sum:             " << htons(ip_header->ip_sum) << std::endl;
      std::cout << "ip_src:             " << inet_ntoa(ip_header->ip_src) << std::endl;
      std::cout << "ip_dst:             " << inet_ntoa(ip_header->ip_dst) << std::endl;
      std::cout << "----------"           << std::endl;
      auto udp_header = reinterpret_cast<udphdr*>(buf + sizeof(ip) + 4);
      std::cout << "size of udp header: " << sizeof(udphdr) << std::endl;
      std::cout << "uh_sport:           " << htons(udp_header->uh_sport) << std::endl;
      std::cout << "uh_dport:           " << htons(udp_header->uh_dport) << std::endl;
      std::cout << "un_ulen:            " << htons(udp_header->uh_ulen) << std::endl;
      std::cout << "uh_sum:             " << htons(udp_header->uh_sum) << std::endl;
      std::cout << "--------------------------------------------------" << std::endl;
      std::cout << "n:                  " << n << std::endl;
      auto offset = sizeof(ip) + 4 + sizeof(udphdr);
      std::cout << "buf:                " << buf + offset << std::endl;
      std::cout << std::endl;
    }
  }

  return 0;
}
