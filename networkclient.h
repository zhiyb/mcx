#pragma once

#include <uv.h>

#define BUFFER_SIZE	4096

class Network;

class NetworkClient
{
public:
	NetworkClient(uv_stream_t *server);
	~NetworkClient();

	void connectTo(const char *name, const char *port);

private:
	// Callbacks
	static void getServerInfo(uv_getaddrinfo_t *req,
			int status, struct addrinfo *res);
	static void remoteConnect(uv_connect_t *req, int status);

	Network *n = 0;
	uv_tcp_t *client = 0, *remote = 0;
	uv_connect_t connect;
	struct {
		char buf[BUFFER_SIZE];
	} cbuf, rbuf;
};
