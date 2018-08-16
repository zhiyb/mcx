#pragma once

#include <uv.h>
#include "parser.h"
#include "../networkrequests.h"
#include "../buffer.h"

class ParserMCX: public Parser
{
public:
	ParserMCX(Client *c): Parser(c) {}
	~ParserMCX();

	virtual std::string name() {return "MCX";}
	virtual bool shutdown();
	virtual bool identify(void *buf, size_t len);

	virtual void init();
	virtual void read(std::vector<char> *buf);

private:
	void remoteInfo(uv_getaddrinfo_t *req, int status);
	void remoteConnect();
	void remoteAlloc(size_t suggested_size, uv_buf_t *buf);
	void remoteRead(ssize_t nread, const uv_buf_t *buf);
	void remoteWrite(std::vector<char> *buf);
	void remoteWrite(uv_write_t *req);
	void remoteClose();

	// Callbacks
	static void remoteInfo(uv_getaddrinfo_t *req,
			int status, struct addrinfo *res);
	static void remoteConnect(uv_connect_t *req, int status);
	static void remoteAlloc(uv_handle_t *handle,
			size_t suggested_size, uv_buf_t *buf);
	static void remoteRead(uv_stream_t *stream,
			ssize_t nread, const uv_buf_t *buf);
	static void remoteWrite(uv_write_t *req, int status);
	static void remoteClose(uv_handle_t *handle);

	NetworkRequests reqs;
	Buffer<char> rbuf, cbuf;
	uv_tcp_t *remote = 0;
	bool _shutdown = false;
};
