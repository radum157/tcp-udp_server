#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netinet/tcp.h>

#include "utils.h"
#include "app_prot.h"

#define CLIENT_SHUTDOWN -1
#define EVENT_HANDLED 0

#define BUFFER_SIZE BUFSIZ

#define INT_MSG 0
#define SHRT_MSG 1
#define FLOAT_MSG 2
#define STR_MSG 3

#define STDIN_POS 0
#define SOCK_POS 1

/**
 * Initialise @a tcp_msg_t structure
*/
void init_message(tcp_msg_t *msg, char id[ID_MAX_LEN], uint8_t type,
	uint16_t len, char *payload)
{
	memcpy(msg->hdr.id, id, ID_MAX_LEN);
	msg->hdr.type = type;
	msg->hdr.len = htons(len + 1 + sizeof(tcp_header_t));

	if (payload != NULL) {
		memcpy(msg->payload, payload, len);
	}
}

/**
 * Handles subscription commands
*/
void handle_subscription(char buf[BUFFER_SIZE], int sockfd, char id[ID_MAX_LEN])
{
	char topic[BUFFER_SIZE];
	scanf("%s", topic);

	tcp_msg_t msg = { .payload = { 0 } };

	if (strcmp(buf, "subscribe") == 0) {
		init_message(&msg, id, CLIENT_SUBSCRIBE, strlen(topic), topic);
		transfer_fixed((char *)&msg, sockfd, ntohs(msg.hdr.len), send);

		printf("Subscribed to topic %s\n", topic);
	} else if (strcmp(buf, "unsubscribe") == 0) {
		init_message(&msg, id, CLIENT_UNSUBSCRIBE, strlen(topic), topic);
		transfer_fixed((char *)&msg, sockfd, ntohs(msg.hdr.len), send);

		printf("Unsubscribed from topic %s\n", topic);
	}
}

void print_message(udp_msg_t udp_msg)
{
	static const char *types[] = {
		"INT", "SHORT_REAL", "FLOAT", "STRING"
	};

	char *msg = udp_msg.payload;

	uint8_t sgn = 0, pwr;
	uint32_t nr;
	int32_t res_int;
	double res_dbl = 0;

	switch (udp_msg.hdr.type) {
	case INT_MSG:
		sgn = *msg;
		nr = *((uint32_t *)(msg + 1));
		res_int = ntohl(nr) * ((sgn != 0) ? -1 : 1);

		printf("%s - %s - %d\n", udp_msg.hdr.topic,
			types[udp_msg.hdr.type], res_int);
		break;

	case SHRT_MSG:
		nr = ntohs(*((uint16_t *)msg));
		res_dbl = (double)nr / 100.0;

		printf("%s - %s - %.2lf\n", udp_msg.hdr.topic,
			types[udp_msg.hdr.type], res_dbl);
		break;

	case FLOAT_MSG:
		sgn = *msg;
		nr = ntohl(*((uint32_t *)(msg + 1)));
		pwr = *((uint8_t *)(msg + sizeof(uint32_t) + 1));

		for (; pwr != 0; pwr--) {
			res_dbl += nr % 10;
			nr /= 10;
			res_dbl /= 10.0;
		}

		res_dbl += nr;
		res_dbl *= (sgn != 0) ? -1 : 1;

		printf("%s - %s - %lf\n", udp_msg.hdr.topic,
			types[udp_msg.hdr.type], res_dbl);
		break;

	case STR_MSG:
		printf("%s - %s - %s\n", udp_msg.hdr.topic,
			types[udp_msg.hdr.type], msg);
		break;

	default:
		break;
	}
}

int handle_message(int fd)
{
	udp_msg_t msg = { .payload = { 0 } };

	ssize_t len = transfer_fixed((char *)&msg.hdr, fd, sizeof(udp_header_t), recv);
	if (len == 0) {
		return CLIENT_SHUTDOWN;
	}

	len = transfer_fixed(msg.payload, fd,
		ntohs(msg.hdr.len) - sizeof(udp_header_t), recv);

	/**
	 * Optional: print IP and port of sender
	 *
	 * inet_addr(msg.sender.sin_addr)
	 * ntohs(msg.sender.sin_port)
	*/
	print_message(msg);
	return EVENT_HANDLED;
}

/**
 * Handles events on the socket and stdin
*/
int std_event(struct pollfd poll_fds[2], char id[ID_MAX_LEN])
{
	/* stdin */
	if (poll_fds[STDIN_POS].revents & POLLIN) {
		char buf[BUFFER_SIZE] = { 0 };
		scanf("%s", buf);

		if (strcmp(buf, "exit") == 0) {
			return CLIENT_SHUTDOWN;
		} else {
			handle_subscription(buf, poll_fds[SOCK_POS].fd, id);
		}
	}

	if (poll_fds[SOCK_POS].revents & POLLIN) {
		return handle_message(poll_fds[SOCK_POS].fd);
	}
	return EVENT_HANDLED;
}

/**
 * Connect message
*/
void server_connect(int fd, char id[ID_MAX_LEN])
{
	tcp_msg_t msg = { .payload = { 0 } };
	init_message(&msg, id, CLIENT_CONNECT, sizeof(tcp_header_t), NULL);

	transfer_fixed((char *)&msg, fd, ntohs(msg.hdr.len), send);
}

/**
 * Client setup
*/
int main(int argc, char const *argv[])
{
	DIE(argc != 4, "Usage: ./subscriber <ID> <SERVER IP> <SERVER PORT>");
	DIE(strlen(argv[1]) > ID_MAX_LEN, "Invalid id");

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	char id[ID_MAX_LEN] = { 0 };
	memcpy(id, argv[1], strlen(argv[1]));

	/* Get server data */
	in_addr_t addr = inet_addr(argv[2]);
	in_port_t port = atoi(argv[3]);

	/* Create socket */
	int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	DIE(sockfd < 0, "Socket creation failed");

	/* Disable Nagle */
	int flag = 1;
	int rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
	DIE(rc < 0, "Setsockopt failed");

	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_in sock_addr = { .sin_zero = { 0 } };

	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(port);
	sock_addr.sin_addr.s_addr = addr;

	rc = connect(sockfd, (struct sockaddr *)&sock_addr, slen);
	DIE(rc < 0, "Cannot connect to server");

	server_connect(sockfd, id);

	/* Multiplexing */
	struct pollfd poll_fds[2] = {
		{ .events = POLLIN, .fd = STDIN_FILENO },
		{ .events = POLLIN, .fd = sockfd }
	};

	/* Main loop */
	while (1) {
		rc = poll(poll_fds, 2, -1);
		DIE(rc < 0, "Poll error");

		if (std_event(poll_fds, id) == CLIENT_SHUTDOWN) {
			break;
		}
	}

	close(sockfd);

	return 0;
}

