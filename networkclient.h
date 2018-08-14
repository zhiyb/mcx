#pragma once

#include <vector>
#include <uv.h>
#include "networkrequests.h"
#include "client.h"
#include "buffer.h"

#define BUFFER_SIZE	4096

class Network;

class NetworkClient
{
public:
	NetworkClient(Network *n);
	~NetworkClient();

	void accept(uv_stream_t *server);
	void connectTo(const char *name, const char *port);
	void close();

	void write(bool client, uv_buf_t buf);

private:
	void read(bool client);

	// Callbacks
	static void alloc(uv_handle_t *handle,
			size_t suggested_size, uv_buf_t *buf);
	static void read(uv_stream_t *stream,
			ssize_t nread, const uv_buf_t *buf);
	static void write(uv_write_t *req, int status);
	static void getServerInfo(uv_getaddrinfo_t *req,
			int status, struct addrinfo *res);
	static void remoteConnect(uv_connect_t *req, int status);
	static void close(uv_handle_t *handle);

	Network *n = 0;
	NetworkRequests reqs;
	Client c;
	uv_tcp_t *client = 0, *remote = 0;
	uv_connect_t connect;
	Buffer<char> cbuf, rbuf;
	//std::list<std::vector<char> *> cbuf, rbuf;
	bool shutdown = false;
};
