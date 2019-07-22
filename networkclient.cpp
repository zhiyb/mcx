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
	if (client) {
		LOG(error, "Destructed with client connection");
		uv_close((uv_handle_t *)client, 0);
		delete client;
	}
	n->removeClient(this);
}

Config &NetworkClient::config() const
{
	return n->config();
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
		LOG(warn, "{}", uv_strerror(err));
	} else {
		try {
			LOG(info, "Client {} from {}",
					(void *)this, Network::getString(pa));
		} catch (std::exception &e) {
			LOG(warn, "{}", e.what());
		}
	}

	if ((err = uv_tcp_nodelay(client, 1)))
		LOG(warn, "{}", uv_strerror(err));

	this->client = client;
	c.init(this, server->loop);
	read();
}

void NetworkClient::close()
{
	// Gracefully shutdown
	if (c.shutdown() && !reqs.busy() && !client) {
		LOG(info, "Client {} connection closed", (void *)this);
		delete this;
	} else if (!shutdown) {
		shutdown = true;
		reqs.cancelAll();
		if (client) {
			uv_read_stop((uv_stream_t *)client);
			uv_close((uv_handle_t *)client, close);
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
	nc->cbuf.enqueue(pbuf);
}

void NetworkClient::read()
{
	int err = uv_read_start((uv_stream_t *)client, alloc, read);
	readStopped = !!err;
	if (err)
		throw std::runtime_error(uv_strerror(err));
}

void NetworkClient::read(uv_stream_t *stream,
		ssize_t nread, const uv_buf_t *buf)
{
	NetworkClient *nc = (NetworkClient *)stream->data;
	bool client = stream == (uv_stream_t *)nc->client;
	if (nread < 0) {
		LOG(warn, "{}", uv_strerror(nread));
		nc->close();
		return;
	}
	if (!client)
		return;

	// Dequeue buffer
	std::vector<char> *p = 0;
	while ((p = nc->cbuf.dequeue()) != 0 && p->data() != buf->base) {
		LOG(warn, "Buffer out-of-order");
		delete p;
	}
	if (!p)
		LOG(warn, "Buffer invalid");
	else
		nc->c.read(p);
}

void NetworkClient::readStop()
{
	uv_read_stop((uv_stream_t *)client);
	readStopped = true;
}

void NetworkClient::write(std::vector<char> *buf)
{
	uv_write_t *req = new uv_write_t;
	req->data = buf;
	reqs += req;
	uv_buf_t wbuf = {.base = buf->data(), .len = buf->size()};
	int err = uv_write(req, (uv_stream_t *)client, &wbuf, 1, write);
	if (err) {
		reqs -= req;
		LOG(warn, "{}", uv_strerror(err));
		delete req;
		delete buf;
		close();
	}
}

void NetworkClient::write(uv_write_t *req, int status)
{
	NetworkClient *nc = (NetworkClient *)req->handle->data;
	nc->reqs -= req;
	delete (std::vector<char> *)req->data;
	delete req;
}

void NetworkClient::close(uv_handle_t *handle)
{
	NetworkClient *nc = (NetworkClient *)handle->data;
	delete handle;
	nc->client = 0;
	nc->close();
}
