#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a socket */
	int fd;

	/* a network interface */
	struct ifreq interface;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* create a socket */
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == fd)
		goto end;

	/* get the interface flags */
	(void) strncpy(interface.ifr_name, argv[1], sizeof(interface.ifr_name));
	if (-1 == ioctl(fd, SIOCGIFFLAGS, &interface))
		goto close_socket;

	/* set the interface flags */
	interface.ifr_flags |= (IFF_UP | IFF_RUNNING);
	if (-1 == ioctl(fd, SIOCSIFFLAGS, &interface))
		goto close_socket;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the socket */
	(void) close(fd);

end:
	return exit_code;
}
