#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the action to perform */
	int action;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* decide which action to perform */
	if (0 == strcmp("off", argv[1]))
		action = 6;
	else {
		if (0 == strcmp("on", argv[1]))
			action = 7;
		else
			goto end;
	}

	/* open the kernel log */
	if (-1 == klogctl(1, NULL, 0))
		goto end;

	/* perform the requested action */
	if (-1 == klogctl(action, NULL, 0))
		goto close_kernel_log;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_kernel_log:
	/* close the kernel log */
	(void) klogctl(0, NULL, 0);

end:
	return exit_code;
}
