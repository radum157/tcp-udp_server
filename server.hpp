#ifndef __SERVER_HPP
#define __SERVER_HPP 1

#include <map>
#include <set>

struct TcpHandler {
private:
	void _print_connect(const std::string &id, int fd);

public:

	std::map<std::string, std::set<std::string>> topics;
	std::map<std::string, int> ids;
	std::map<int, std::string> fds;

	/* Disconnects the client with the given fd */
	std::string disconnect(int fd);

	/* Connects a client with the given id and fd */
	void connect(const std::string &id, struct pollfd &pfd);

	inline void subscribe(const std::string &topic, const std::string &id)
	{
		topics[id].insert(topic);
	}

	inline void unsubscribe(const std::string &topic, const std::string &id)
	{
		topics[id].erase(topic);
	}

	void notifyAll(const struct udp_msg_t &msg) const;

};

#endif
