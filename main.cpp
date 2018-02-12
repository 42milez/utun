#include <iostream>

#include <sys/kern_control.h>


#include <netinet/in.h>

#include <net/if_utun.h>
#include <sys/ioctl.h>
#include <sys/kern_event.h>
#include <unistd.h>
#include <fcntl.h>

#include <arpa/inet.h>

int open_utun() {
  sockaddr_ctl addr{};
  ctl_info info{};
  int err;

  // Protocol families are mapped onto address families
  // --------------------------------------------------
  // Notes:
  //  - KEXT Controls and Notifications
  //    https://developer.apple.com/library/content/documentation/Darwin/Conceptual/NKEConceptual/control/control.html
  auto soc = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
  if (soc < 0) {
    return soc;
  }

  // set the socket non-blocking
  err = fcntl(soc, F_SETFL, O_NONBLOCK);
  if (err != 0) {
    close(soc);
    return err;
  }

  // set close-on-exec flag
  auto flags = fcntl(soc, F_GETFD);
  flags |= FD_CLOEXEC;
  err = fcntl(soc, F_SETFD, flags);
  if (err != 0) {
    close(soc);
    return err;
  }

  // Convert a kernel control name to a kernel control id
  // --------------------------------------------------
  // Notes:
  //  - CTLIOCGINFO
  //    https://developer.apple.com/documentation/kernel/ctliocginfo?language=objc
  strncpy(info.ctl_name, UTUN_CONTROL_NAME, MAX_KCTL_NAME);
  err = ioctl(soc, CTLIOCGINFO, &info);
  if (err != 0) {
    close(soc);
    return err;
  }

  addr.sc_len     = sizeof(addr);
  addr.sc_family  = AF_SYSTEM;
  addr.ss_sysaddr = AF_SYS_CONTROL;
  addr.sc_id      = info.ctl_id;
  addr.sc_unit    = 0;

  err = connect(soc, (struct sockaddr *)&addr, sizeof(addr));
  if (err != 0) {
   close(soc);
   return err;
  }

  return soc;
}

int main() {
  int soc = open_utun();
  if (soc >= 0) {
    printf("A tun interface has been created.\n");
  }

  sockaddr_in ip4_addr{};

  ip4_addr.sin_family = AF_INET;
  ip4_addr.sin_port = htons(3490);
  inet_pton(AF_INET, "10.0.100.1", &ip4_addr.sin_addr);

  // bind an address
  auto err = bind(soc, (sockaddr *)&ip4_addr, sizeof(ip4_addr));
  if (err != 0) {
    std::cout << "Failed to bind a local address" << std::endl;
    return -1;
  }

  char s[100];
  while (scanf("%s", s) != EOF) {
    if (s[0] == 'q') break;
    sleep(1);
  }

  close(soc);
}
