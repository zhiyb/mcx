#pragma once

#include <string>
#include <list>
#include <uv.h>
#include "networkrequests.h"
#include "mutex.h"

class Config;
class NetworkClient;

class Network
{
public:
	Network(uv_loop_t *loop, Config *cfg);
	~Network();

	Config &config() {return *cfg;}

	NetworkRequests *requests() {return &reqs;}
	void addClient(NetworkClient *client);
	void removeClient(NetworkClient *client);
	static std::string getString(const struct sockaddr *addr);

private:
	int test(const char *str);
	// Callbacks
	static void getServerInfo(uv_getaddrinfo_t *req,
			int status, struct addrinfo *res);
	static void serverAccept(uv_stream_t *server, int status);

	uv_tcp_t *server = 0;
	std::list<NetworkClient *> clients;
	Mutex clients_mutex;
	NetworkRequests reqs;
	Config *cfg;
};
