#include <stdexcept>
#include "logging.h"
#include "networkclient.h"
#include "network.h"

NetworkClient::NetworkClient(Network *n)
{
	this->n = n;
	n->addClient(this);
}

NetworkClient::~NetworkClient()
{
	if (remote) {
		logger->error("Destructed with remote connection");
		uv_close((uv_handle_t *)remote, 0);
		delete remote;
	}
	if (client) {
		logger->error("Destructed with client connection");
		uv_close((uv_handle_t *)client, 0);
		delete client;
	}
	for (auto *pb: cbuf)
		delete pb;
	for (auto *pb: rbuf)
		delete pb;
	n->removeClient(this);
}

void NetworkClient::accept(uv_stream_t *server)
{
	uv_tcp_t *client = new uv_tcp_t;
	int err = 0;
	client->data = this;

	if ((err = uv_tcp_init(server->loop, client))) {
		delete client;
		throw std::runtime_error(uv_strerror(err));
	}

	if ((err = uv_accept(server, (uv_stream_t *)client))) {
		delete client;
		throw std::runtime_error(uv_strerror(err));
	}

	// Print peer name
	struct sockaddr_storage addr;
	struct sockaddr *pa = (struct sockaddr *)&addr;
	int len = sizeof(addr);
	if ((err = uv_tcp_getpeername(client, pa, &len))) {
		logger->warn("{}", uv_strerror(err));
	} else {
		try {
			logger->info("Client {} from {}",
					(void *)this, Network::getString(pa));
		} catch (std::exception &e) {
			logger->warn("{}", e.what());
		}
	}

	if ((err = uv_tcp_nodelay(client, 1)))
		logger->warn("{}", uv_strerror(err));

	this->client = client;
	read(true);
}

void NetworkClient::connectTo(const char *name, const char *port)
{
	// Remote server name resolution
	uv_getaddrinfo_t *reqAddrInfo = new uv_getaddrinfo_t;
	reqAddrInfo->data = this;
	reqs.addRequest((uv_req_t *)reqAddrInfo);
	int err = uv_getaddrinfo(client->loop, reqAddrInfo, getServerInfo,
			name, port, 0);
	if (err)
		throw std::runtime_error(uv_strerror(err));
}

void NetworkClient::close()
{
	// Gracefully shutdown
	if (!reqs.busy() && !client && !remote) {
		logger->info("Client {} connection closed", (void *)this);
		delete this;
	} else if (!shutdown) {
		shutdown = true;
		reqs.cancelAll();
		if (client) {
			uv_read_stop((uv_stream_t *)client);
			uv_close((uv_handle_t *)client, close);
		}
		if (remote) {
			uv_read_stop((uv_stream_t *)remote);
			uv_close((uv_handle_t *)remote, close);
		}
	}
}

void NetworkClient::alloc(uv_handle_t *handle,
		size_t suggested_size, uv_buf_t *buf)
{
	auto *pbuf = new std::vector<char>(suggested_size);
	buf->base = pbuf->data();
	buf->len = pbuf->size();

	NetworkClient *nc = (NetworkClient *)handle->data;
	bool client = handle == (uv_handle_t *)nc->client;
	auto &vbuf = client ? nc->cbuf : nc->rbuf;
	vbuf.push_back(pbuf);
}

void NetworkClient::read(bool client)
{
	auto *ps = (uv_stream_t *)(client ? this->client : this->remote);
	int err = uv_read_start(ps, alloc, read);
	if (err)
		throw std::runtime_error(uv_strerror(err));
}

void NetworkClient::read(uv_stream_t *stream,
		ssize_t nread, const uv_buf_t *buf)
{
	NetworkClient *nc = (NetworkClient *)stream->data;
	if (nread < 0) {
		logger->warn("{}", uv_strerror(nread));
		nc->close();
		return;
	}

	logger->debug("{}: {} bytes read", __func__, nread);
	nc->write(stream != (uv_stream_t *)nc->client, *buf);
}

void NetworkClient::write(bool client, uv_buf_t buf)
{
	auto *ps = (uv_stream_t *)(client ? this->client : this->remote);
	if (!ps)
		return;
	uv_write_t *req = new uv_write_t;
	req->data = buf.base;
	reqs.addRequest((uv_req_t *)req);
	int err = uv_write(req, ps, &buf, 1, write);
	if (err) {
		reqs.removeRequest((uv_req_t *)req);
		logger->warn("{}", uv_strerror(err));
		delete req;
		close();
	}
}

void NetworkClient::write(uv_write_t *req, int status)
{
	NetworkClient *nc = (NetworkClient *)req->handle->data;
	bool client = req->handle == (uv_stream_t *)nc->client;
	void *base = req->data;
	nc->reqs.removeRequest((uv_req_t *)req);
	delete req;

	if (status) {
		logger->warn("{}", uv_strerror(status));
		nc->close();
		return;
	}

	// Buffer should be from the opposite side
	auto &vbuf = client ? nc->rbuf : nc->cbuf;
	if (!vbuf.front()) {
		logger->warn("Buffer invalid");
	} else if (base == vbuf.front()->data()) {
		delete vbuf.front();
		vbuf.pop_front();
	} else {
		logger->warn("Buffer out-of-order");
		vbuf.remove_if([=](auto *pb) {
			if (pb->data() == base) {
				delete pb;
				return true;
			} else {
				return false;
			}});
	}
}

void NetworkClient::getServerInfo(uv_getaddrinfo_t *req,
		int status, struct addrinfo *res)
{
	NetworkClient *nc = static_cast<NetworkClient *>(req->data);
	nc->reqs.removeRequest((uv_req_t *)req);
	if (status != 0) {
		delete req;
		throw std::runtime_error(uv_strerror(status));
	}

	try {
		// Remote server connection
		uv_tcp_t *remote = new uv_tcp_t;
		nc->remote = remote;
		remote->data = nc;
		nc->connect.data = nc;

		struct addrinfo *pa;
		for (pa = res; pa != 0; pa = pa->ai_next) {
			int err = 0;
			if ((err = uv_tcp_init(req->loop, remote))) {
				logger->warn("{}", uv_strerror(err));
				continue;
			}

			nc->reqs.addRequest((uv_req_t *)&nc->connect);
			if ((err = uv_tcp_connect(&nc->connect, remote,
					pa->ai_addr, remoteConnect))) {
				nc->reqs.removeRequest(
						(uv_req_t *)&nc->connect);
				logger->warn("{}", uv_strerror(err));
				uv_close((uv_handle_t *)remote, 0);
				continue;
			}
			break;
		}

		if (!pa)
			throw std::runtime_error(
					"No usable remote server address");
	} catch (std::exception &e) {
		uv_freeaddrinfo(req->addrinfo);
		delete req;
		delete nc->remote;
		nc->remote = 0;
		throw e;
	}

	uv_freeaddrinfo(req->addrinfo);
	delete req;
	nc->read(false);
}

void NetworkClient::remoteConnect(uv_connect_t *req, int status)
{
	NetworkClient *nc = static_cast<NetworkClient *>(req->data);
	nc->reqs.removeRequest((uv_req_t *)req);
	if (status)
		throw std::runtime_error(uv_strerror(status));

	// Print peer name
	struct sockaddr_storage addr;
	struct sockaddr *pa = (struct sockaddr *)&addr;
	int len = sizeof(addr);
	if ((status = uv_tcp_getpeername(nc->remote, pa, &len))) {
		logger->warn("{}", uv_strerror(status));
	} else {
		try {
			logger->info("Connected to {}", Network::getString(pa));
		} catch (std::exception &e) {
			logger->warn("{}", e.what());
		}
	}
}

void NetworkClient::close(uv_handle_t *handle)
{
	NetworkClient *nc = (NetworkClient *)handle->data;
	bool client = handle == (uv_handle_t *)nc->client;
	auto **ps = client ? &nc->client : &nc->remote;
	*ps = 0;
	delete handle;
	nc->close();
}
