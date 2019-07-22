#include <cstring>
#include <stdexcept>
#include "../logging.h"
#include "../config.h"
#include "../network.h"
#include "../stream.h"
#include "../callback.h"
#include "mcx.h"

ParserMCX::~ParserMCX()
{
	if (_remote)
		delete _remote;
	if (remote) {
		LOG(error, "Destructed with remote connection");
		uv_close((uv_handle_t *)remote, 0);
		delete remote;
	}
}

bool ParserMCX::identify(void *buf, size_t len)
{
	LOG(debug, "{}", __PRETTY_FUNCTION__);
	return true;
}

void ParserMCX::init()
{
	const char *name = config()["mcx.remote.host"].c_str();
	const char *port = config()["mcx.remote.port"].c_str();
	if (!*name || !*port)
		throw std::runtime_error("Invalid configration");

	// Remote server name resolution
	uv_getaddrinfo_t *reqAddrInfo = new uv_getaddrinfo_t;
	reqAddrInfo->data = this;
	reqs += reqAddrInfo;
	int err = uv_getaddrinfo(loop(), reqAddrInfo, remoteInfo,
			name, port, 0);
	if (err)
		throw std::runtime_error(uv_strerror(err));
}

bool ParserMCX::shutdown()
{
	if (_remote && !_remote->shutdown())
		return false;

	bool shutdown = _shutdown;
	_shutdown = true;

	if (!reqs && !remote)
		return true;

	if (!shutdown) {
		reqs.cancelAll();
		if (remote) {
			uv_read_stop((uv_stream_t *)remote);
			uv_close((uv_handle_t *)remote, remoteClose);
		}
	}
	return false;
}

void ParserMCX::read(std::vector<char> *buf)
{
	if (cbuf) {
		cbuf.enqueue(buf);
		if (remote) {
			cbuf.combine();
			remoteWrite(cbuf.dequeue());
		}
	} else if (remote) {
		remoteWrite(buf);
	} else {
		cbuf.enqueue(buf);
	}
}

void ParserMCX::remoteInfo(uv_getaddrinfo_t *req, int status)
{
	uv_connect_t *connect = new uv_connect_t;
	connect->data = this;
	try {
		// Remote server connection
		remote = new uv_tcp_t;
		remote->data = this;

		struct addrinfo *pa, *res = req->addrinfo;
		for (pa = res; pa != 0; pa = pa->ai_next) {
			int err = 0;
			if ((err = uv_tcp_init(loop(), remote))) {
				LOG(warn, "{}", uv_strerror(err));
				continue;
			}

			reqs += connect;
			if ((err = uv_tcp_connect(connect, remote,
					pa->ai_addr, remoteConnect))) {
				reqs -= connect;
				LOG(warn, "{}", uv_strerror(err));
				uv_close((uv_handle_t *)remote, 0);
				continue;
			}
			break;
		}

		if (!pa)
			throw std::runtime_error(
					"No usable remote server address");
	} catch (std::exception &e) {
		delete connect;
		delete remote;
		remote = 0;
		uv_freeaddrinfo(req->addrinfo);
		delete req;
		throw e;
	}

	uv_freeaddrinfo(req->addrinfo);
	delete req;
}

void ParserMCX::remoteConnect()
{
	// Print peer name
	struct sockaddr_storage addr;
	struct sockaddr *pa = (struct sockaddr *)&addr;
	int len = sizeof(addr);
	int err = uv_tcp_getpeername(remote, pa, &len);
	if (err) {
		LOG(warn, "{}", uv_strerror(err));
	} else {
		try {
			LOG(info, "Connected to {}", Network::getString(pa));
		} catch (std::exception &e) {
			LOG(warn, "{}", e.what());
		}
	}

	if ((err = uv_tcp_nodelay(remote, 1)))
		LOG(warn, "{}", uv_strerror(err));

	_remote = new Stream((uv_stream_t *)remote,
			stoi(config()["mcx.remote.buffers"]));

	_remote->readCallbacks().add(this, &ParserMCX::test);

	// Remote read start
	if ((err = uv_read_start((uv_stream_t *)remote,
			remoteAlloc, remoteRead)))
		throw std::runtime_error(uv_strerror(err));

	// Write stored content
	if (cbuf) {
		cbuf.combine();
		remoteWrite(cbuf.dequeue());
	}
}

void ParserMCX::remoteAlloc(size_t suggested_size, uv_buf_t *buf)
{
	auto *pbuf = new std::vector<char>(suggested_size);
	buf->base = pbuf->data();
	buf->len = pbuf->size();
	rbuf.enqueue(pbuf);
}

void ParserMCX::remoteRead(ssize_t nread, const uv_buf_t *buf)
{
	if (nread < 0) {
		LOG(warn, "{}", uv_strerror(nread));
		close();
		return;
	}

	// Dequeue buffer
	std::vector<char> *p = 0;
	while ((p = rbuf.dequeue()) != 0 && p->data() != buf->base) {
		LOG(warn, "Buffer out-of-order");
		delete p;
	}
	if (!p) {
		LOG(warn, "Buffer invalid");
	} else {
		p->resize(nread);
		write(p);
	}
}

void ParserMCX::remoteWrite(std::vector<char> *buf)
{
	uv_write_t *req = new uv_write_t;
	req->data = buf;
	reqs += req;
	uv_buf_t wbuf = {.base = buf->data(), .len = buf->size()};
	int err = uv_write(req, (uv_stream_t *)remote, &wbuf, 1, remoteWrite);
	if (err) {
		reqs -= req;
		LOG(warn, "{}", uv_strerror(err));
		delete req;
		delete buf;
		close();
	}
}

void ParserMCX::remoteWrite(uv_write_t *req)
{
	reqs -= req;
	delete (std::vector<char> *)req->data;
	delete req;
}

void ParserMCX::remoteClose()
{
	LOG(debug, "{}, {}, {}", __PRETTY_FUNCTION__, !!reqs, !!remote);
	delete remote;
	remote = 0;
	close();
}

void ParserMCX::remoteInfo(uv_getaddrinfo_t *req,
		int status, struct addrinfo *res)
{
	ParserMCX *p = static_cast<ParserMCX *>(req->data);
	p->reqs -= req;
	if (status) {
		delete req;
		throw std::runtime_error(uv_strerror(status));
	}
	p->remoteInfo(req, status);
}

void ParserMCX::remoteConnect(uv_connect_t *req, int status)
{
	ParserMCX *p = static_cast<ParserMCX *>(req->data);
	p->reqs -= req;
	delete req;
	if (status)
		throw std::runtime_error(uv_strerror(status));
	p->remoteConnect();
}

void ParserMCX::remoteAlloc(uv_handle_t *handle,
		size_t suggested_size, uv_buf_t *buf)
{
	ParserMCX *p = static_cast<ParserMCX *>(handle->data);
	p->remoteAlloc(suggested_size, buf);
}

void ParserMCX::remoteRead(uv_stream_t *stream,
		ssize_t nread, const uv_buf_t *buf)
{
	ParserMCX *p = static_cast<ParserMCX *>(stream->data);
	p->remoteRead(nread, buf);
}

void ParserMCX::remoteWrite(uv_write_t *req, int status)
{
	ParserMCX *p = static_cast<ParserMCX *>(req->handle->data);
	p->remoteWrite(req);
}

void ParserMCX::remoteClose(uv_handle_t *handle)
{
	ParserMCX *p = static_cast<ParserMCX *>(handle->data);
	p->remoteClose();
}
