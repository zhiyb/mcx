#pragma once

#include <vector>
#include <uv.h>
#include "networkrequests.h"
#include "client.h"
#include "buffer.h"

#define BUFFER_SIZE	4096

class Config;
class Network;

class NetworkClient
{
public:
	NetworkClient(Network *n);
	~NetworkClient();

	Config &config() const;

	void accept(uv_stream_t *server);
	void close();

	void read();
	bool readActive() {return !readStopped;}
	void readStop();
	void write(std::vector<char> *buf);

private:
	// Callbacks
	static void alloc(uv_handle_t *handle,
			size_t suggested_size, uv_buf_t *buf);
	static void read(uv_stream_t *stream,
			ssize_t nread, const uv_buf_t *buf);
	static void write(uv_write_t *req, int status);
	static void close(uv_handle_t *handle);

	Network *n = 0;
	NetworkRequests reqs;
	Client c;
	uv_tcp_t *client = 0;
	uv_connect_t connect;
	Buffer<char> cbuf;
	bool readStopped = false;
	bool shutdown = false;
};
