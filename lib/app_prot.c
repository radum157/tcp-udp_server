#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "app_prot.h"
#include "utils.h"

ssize_t transfer_fixed(char *buf, int sockfd,
	size_t size, void *comm_func)
{
	if (comm_func == NULL) {
		return -1;
	}

	ssize_t(*cfunc)(int, void *, size_t, int) = comm_func;

	ssize_t remain = size;
	ssize_t handled = 0;

	while (remain) {
		ssize_t tmp = cfunc(sockfd, buf + handled, remain, 0);

		DIE(tmp < 0, "Data transfer failed");
		if (tmp == 0) {
			break;
		}

		remain -= tmp;
		handled += tmp;
	}

	return handled;
}
