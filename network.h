#pragma once

#include <string>
#include <list>
#include <uv.h>
#include "networkrequests.h"
#include "mutex.h"

class NetworkClient;

class Network
{
public:
	Network(uv_loop_t *loop, const char *name, const char *port,
			const char *remote_name, const char *remote_port);
	~Network();

	NetworkRequests *requests() {return &reqs;}
	void addClient(NetworkClient *client);
	void removeClient(NetworkClient *client);
	static std::string getString(const struct sockaddr *addr);

private:
	// Callbacks
	static void getServerInfo(uv_getaddrinfo_t *req,
			int status, struct addrinfo *res);
	static void serverAccept(uv_stream_t *server, int status);

	std::string remote_name, remote_port;
	uv_tcp_t *server = 0;
	Mutex clients_mutex;
	std::list<NetworkClient *> clients;
	NetworkRequests reqs;
};
