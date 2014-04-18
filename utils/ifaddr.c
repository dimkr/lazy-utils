#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>

/* taken from linux/ipv6.h */
struct _in6_ifreq {
	struct in6_addr ifr6_addr;
	uint32_t ifr6_prefixlen;
	unsigned int ifr6_ifindex;
};

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a socket */
	int fd;

	/* a network interface */
	struct ifreq interface;

	/* an IPv6 network interface */
	struct _in6_ifreq ipv6_interface;

	/* the assigned address */
	void *address;

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* convert the address to binary form */
	if (1 == inet_pton(AF_INET,
	                   argv[2],
	                   &((struct sockaddr_in *) &interface.ifr_addr)->sin_addr))
		interface.ifr_addr.sa_family = AF_INET;
	else {
		if (1 != inet_pton(
		             AF_INET6,
		             argv[2],
		             &((struct sockaddr_in6 *) &interface.ifr_addr)->sin6_addr))
			goto end;
		interface.ifr_addr.sa_family = AF_INET6;
	}

	/* set the interface name */
	(void) strncpy(interface.ifr_name, argv[1], sizeof(interface.ifr_name));

	/* create a socket */
	fd = socket(interface.ifr_addr.sa_family, SOCK_DGRAM, 0);
	if (-1 == fd)
		goto end;

	if (AF_INET == interface.ifr_addr.sa_family)
		address = &interface;
	else {
		/* get the interface index */
		if (-1 == ioctl(fd, SIOGIFINDEX, &interface))
			goto close_socket;

		/* copy the interface index and the address */
		(void) memcpy(
		             &ipv6_interface.ifr6_addr,
		             &((struct sockaddr_in6 *) &interface.ifr_addr)->sin6_addr,
		             sizeof(ipv6_interface.ifr6_addr));
		ipv6_interface.ifr6_ifindex = interface.ifr_ifindex;
		ipv6_interface.ifr6_prefixlen = 64;
		address = &ipv6_interface;
	}

	/* set the interface address */
	if (-1 == ioctl(fd, SIOCSIFADDR, address))
		goto close_socket;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the socket */
	(void) close(fd);

end:
	return exit_code;
}
