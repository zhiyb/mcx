#include <stdexcept>
#include "logging.h"
#include "networkclient.h"
#include "network.h"

NetworkClient::NetworkClient(uv_stream_t *server)
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
			logger->info("Client from {}", Network::getString(pa));
		} catch (std::exception &e) {
			logger->warn("{}", e.what());
		}
	}

	if ((err = uv_tcp_nodelay(client, 1)))
		logger->warn("{}", uv_strerror(err));

	this->client = client;
	n = static_cast<Network *>(server->data);
	n->addClient(this);
}

NetworkClient::~NetworkClient()
{
	if (remote) {
		uv_close((uv_handle_t *)remote, 0);
		delete remote;
	}
	if (client) {
		uv_close((uv_handle_t *)client, 0);
		delete client;
	}
	n->removeClient(this);
}

void NetworkClient::connectTo(const char *name, const char *port)
{
	// Remote server name resolution
	uv_getaddrinfo_t *reqAddrInfo = new uv_getaddrinfo_t;
	reqAddrInfo->data = this;
	n->requests()->addRequest((uv_req_t *)reqAddrInfo);
	int err = uv_getaddrinfo(client->loop, reqAddrInfo, getServerInfo,
			name, port, 0);
	if (err)
		throw std::runtime_error(uv_strerror(err));
}

void NetworkClient::getServerInfo(uv_getaddrinfo_t *req,
		int status, struct addrinfo *res)
{
	NetworkClient *nc = static_cast<NetworkClient *>(req->data);
	nc->n->requests()->removeRequest((uv_req_t *)req);
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

			nc->n->requests()->addRequest((uv_req_t *)&nc->connect);
			if ((err = uv_tcp_connect(&nc->connect, remote,
					pa->ai_addr, remoteConnect))) {
				nc->n->requests()->removeRequest(
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
		uv_freeaddrinfo(res);
		uv_freeaddrinfo(req->addrinfo);
		delete req;
		delete nc->remote;
		nc->remote = 0;
		throw e;
	}

	uv_freeaddrinfo(res);
	uv_freeaddrinfo(req->addrinfo);
	delete req;
}

void NetworkClient::remoteConnect(uv_connect_t *req, int status)
{
	NetworkClient *nc = static_cast<NetworkClient *>(req->data);
	nc->n->requests()->removeRequest((uv_req_t *)req);
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
