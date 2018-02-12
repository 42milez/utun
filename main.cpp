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



int main() {
  auto fd = open_utun(static_cast<u_int32_t >(strtol("10.0.7.1", nullptr, 10) + 1));
  if (fd >= 0) {
    printf("INFO: A tun interface has been created.\n");
  }

  //  sockaddr_in ip4_addr{};
  //
  //  ip4_addr.sin_family = AF_INET;
  //  ip4_addr.sin_port = htons(3490);
  //  inet_pton(AF_INET, "10.0.7.1", &ip4_addr.sin_addr);
  //
  //  if (bind(fd, (sockaddr *)&ip4_addr, sizeof(ip4_addr)) < 0) {
  //    std::cout << "Failed to bind a local address" << std::endl;
  //    return -1;
  //  }

  system("ipconfig set utun10 MANUAL 10.0.7.1 16");

  char s[100];
  while (scanf("%s", s) != EOF) {
    if (s[0] == 'q') break;
    sleep(1);
  }

  system("ipconfig set utun10 NONE");

  close(fd);
}
