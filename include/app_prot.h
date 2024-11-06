#ifndef __APP_PROT_H
#define __APP_PROT_H 1

#define ID_MAX_LEN 10
#define PAYLOAD_MAX_LEN 1500
#define TOPIC_MAX_LEN 50
#define MAX_PACKET_LEN (PAYLOAD_MAX_LEN + TOPIC_MAX_LEN + 1)

#define CLIENT_CONNECT 0
#define CLIENT_SUBSCRIBE 1
#define CLIENT_UNSUBSCRIBE 2

#define UDP_MESSAGE 3

/**
 * Protocol for client-server communication
*/
typedef struct tcp_header_t {
	uint8_t type;
	/* Payload + header length */
	uint16_t len;
	char id[ID_MAX_LEN];
} tcp_header_t;

typedef struct tcp_msg_t {
	tcp_header_t hdr;
	char payload[TOPIC_MAX_LEN + 1];
} tcp_msg_t;

typedef struct udp_header_t {
	/* Payload + header length */
	uint16_t len;
	// struct sockaddr_in sender;
	uint8_t type;

	char topic[TOPIC_MAX_LEN + 1];
} udp_header_t;

typedef struct udp_msg_t {
	udp_header_t hdr;
	char payload[PAYLOAD_MAX_LEN + 1];
} udp_msg_t;

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief
	 * Ensures the transfer of exactly @a size bytes from @a buf
	 * on @a sockfd, using @a comm_func to transfer the data.
	 * comm_func should be equivalent to @a recv or @a send
	*/
	ssize_t transfer_fixed(char *buf, int sockfd,
		size_t size, void *comm_func);

#ifdef __cplusplus
}
#endif

#endif
