#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netinet/tcp.h>

#include <vector>

#include "utils.h"
#include "app_prot.h"
#include "cregex.h"

#include "server.hpp"

#define CONN_Q_SIZE 10
#define STD_CONNS 3

#define SERVER_SHUTDOWN -1
#define EVENT_HANDLED 0

#define STDIN_POS 0
#define TCP_POS 1
#define UDP_POS 2

/**
 * Initialises the pollfd array
*/
void init_fds(std::vector<pollfd> &pfds, int tcp_fd, int udp_fd)
{
	pfds[STDIN_POS] = { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 };
	pfds[TCP_POS] = { .fd = tcp_fd, .events = POLLIN, .revents = 0 };
	pfds[UDP_POS] = { .fd = udp_fd, .events = POLLIN, .revents = 0 };

	/* Disable Nagle */
	int flag = 1;
	int rc = setsockopt(tcp_fd, IPPROTO_TCP, TCP_NODELAY,
		(char *)&flag, sizeof(int));
	DIE(rc < 0, "Setsockopt failed");
}

/**
 * Accepts a TCP connection on the first available @a pollfd
*/
void accept_connection(std::vector<pollfd> &pfds)
{
	int fd = accept(pfds[TCP_POS].fd, NULL, NULL);
	DIE(fd < 0, "Accept failed");

	for (size_t i = STD_CONNS; i < pfds.size(); i++) {
		if (pfds[i].fd == -1) {
			pfds[i].fd = fd;
			return;
		}
	}

	pfds.push_back({ .fd = fd, .events = POLLIN, .revents = 0 });
}

void TcpHandler::notifyAll(const struct udp_msg_t &msg) const
{
	for (auto id : topics) {
		auto found = ids.find(id.first);
		if (found == ids.end() || (*found).second == -1) {
			continue;
		}

		for (auto topic : id.second) {
			if (string_matches(msg.hdr.topic, topic.data())) {
				transfer_fixed((char *)&msg, (*found).second,
					ntohs(msg.hdr.len), (void *)send);
				break;
			}
		}
	}
}

/**
 * Forwards UDP messages
*/
void handle_udp(int sockfd, const TcpHandler &handler)
{
	char buf[MAX_PACKET_LEN];

	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_in saddr;

	ssize_t len = recvfrom(sockfd, buf, MAX_PACKET_LEN, 0,
		(struct sockaddr *)&saddr, &slen);
	DIE(len < 0, "Recvfrom failed");

	if (len == 0) {
		return;
	}

	/* Parse message */
	udp_msg_t msg;
	memset(&msg, 0, sizeof(udp_msg_t));
	// msg.hdr.sender = saddr;

	/* Account for discrepancies in size */
	msg.hdr.len = htons(sizeof(udp_header_t) + (len - TOPIC_MAX_LEN - 1));
	msg.hdr.type = *(buf + TOPIC_MAX_LEN);

	memcpy(msg.hdr.topic, buf, TOPIC_MAX_LEN);
	memcpy(msg.payload, buf + TOPIC_MAX_LEN + 1, len - TOPIC_MAX_LEN - 1);

	handler.notifyAll(msg);
}

/**
 * Handles events on sockets and stdin
*/
int std_event(std::vector<pollfd> &pfds, TcpHandler &handler, int &nr_ev)
{
	/* Stdin */
	if (pfds[STDIN_POS].revents & POLLIN) {
		char buf[BUFSIZ] = { 0 };
		fgets(buf, BUFSIZ - 1, stdin);

		if (memcmp(buf, "exit", sizeof("exit") - 1) == 0) {
			return SERVER_SHUTDOWN;
		}

		nr_ev--;
		if (nr_ev == 0) {
			return EVENT_HANDLED;
		}
	}

	/* TCP */
	if (pfds[TCP_POS].revents & POLLIN) {
		accept_connection(pfds);

		nr_ev--;
		if (nr_ev == 0) {
			return EVENT_HANDLED;
		}
	}

	/* UDP */
	if (pfds[UDP_POS].revents & POLLIN) {
		handle_udp(pfds[UDP_POS].fd, handler);
		nr_ev--;
	}

	return EVENT_HANDLED;
}

std::string TcpHandler::disconnect(int fd)
{
	auto found = fds.find(fd);
	if (found == fds.end()) {
		return "";
	}

	auto alias = ids.find((*found).second);
	std::string ret = (*found).second;

	fds.erase(found);
	(*alias).second = -1;

	return ret;
}

void TcpHandler::_print_connect(const std::string &id, int fd)
{
	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_in saddr;

	getpeername(fd, (struct sockaddr *)&saddr, &slen);

	printf("New client %s connected from %s:%d\n", id.c_str(),
		inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
}

void TcpHandler::connect(const std::string &id, pollfd &pfd)
{
	auto found = ids.find(id);
	if (found == ids.end()) {
		ids.insert({ id, pfd.fd });
		fds.insert({ pfd.fd, id });

		_print_connect(id, pfd.fd);
		return;
	}

	if ((*found).second != -1) {
		printf("Client %s already connected.\n", id.c_str());

		close(pfd.fd);
		pfd.fd = -1;
		return;
	}

	(*found).second = pfd.fd;
	fds.insert({ pfd.fd, id });

	_print_connect(id, pfd.fd);
}

/**
 * TCP communication
*/
void handle_tcp(TcpHandler &handler, pollfd &pfd)
{
	tcp_msg_t msg;
	ssize_t len = transfer_fixed((char *)&msg.hdr, pfd.fd,
		sizeof(tcp_header_t), (void *)recv);

	/* Client disconnect */
	if (len == 0) {
		std::string id = handler.disconnect(pfd.fd);

		close(pfd.fd);
		pfd.fd = -1;

		if (!id.empty()) {
			printf("Client %s disconnected.\n", id.c_str());
		}
		return;
	}

	len = transfer_fixed(msg.payload, pfd.fd,
		ntohs(msg.hdr.len) - sizeof(tcp_header_t), (void *)recv);

	if (msg.hdr.type == CLIENT_CONNECT) {
		handler.connect(msg.hdr.id, pfd);
	} else if (msg.hdr.type == CLIENT_SUBSCRIBE) {
		handler.subscribe(msg.payload, msg.hdr.id);
	} else {
		handler.unsubscribe(msg.payload, msg.hdr.id);
	}
}

/**
 * Events on connected sockets
*/
void other_events(std::vector<pollfd> &pfds, TcpHandler &handler, int &nr_ev)
{
	for (size_t i = STD_CONNS; i < pfds.size(); i++) {
		if (pfds[i].fd == -1 || (pfds[i].revents & POLLIN) == 0) {
			continue;
		}

		handle_tcp(handler, pfds[i]);

		nr_ev--;
		if (nr_ev == 0) {
			return;
		}
	}
}

/**
 * Server setup
*/
int main(int argc, char const *argv[])
{
	DIE(argc != 2, "Usage: ./server <PORT>\n");

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	/* Get port and sockets */
	in_port_t port = atoi(argv[1]);

	int udp_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	int tcp_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	DIE(tcp_sockfd < 0 || tcp_sockfd < 0, "Socket creation failed");

	/* Bind UDP and TCP sockets */
	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_in sock_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = { .s_addr = INADDR_ANY },
		.sin_zero = { 0 }
	};

	int rc = bind(udp_sockfd, (struct sockaddr *)&sock_addr, slen);
	DIE(rc < 0, "Bind failed");

	rc = bind(tcp_sockfd, (struct sockaddr *)&sock_addr, slen);
	DIE(rc < 0, "Bind failed");

	/* Start listening */
	rc = listen(tcp_sockfd, CONN_Q_SIZE);
	DIE(rc < 0, "Listen failed");

	/* Multiplexing */
	std::vector<pollfd> pfds(STD_CONNS);
	init_fds(pfds, tcp_sockfd, udp_sockfd);

	TcpHandler handler;

	/* Main loop */
	while (true) {
		int nr_ev = poll(pfds.data(), pfds.size(), -1);
		DIE(nr_ev < 0, "Poll error");

		rc = std_event(pfds, handler, nr_ev);
		if (rc == SERVER_SHUTDOWN) {
			break;
		}

		other_events(pfds, handler, nr_ev);
	}

	/* Cleanup */
	for (auto pfd : pfds) {
		if (pfd.fd != -1) {
			close(pfd.fd);
		}
	}

	return 0;
}
