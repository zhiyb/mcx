#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdexcept>
#include <iostream>
#include "logging.h"
#include "network.h"
#include "networkclient.h"
#include "config.h"
#include "callback.h"

Network::Network(uv_loop_t *loop, Config *cfg): cfg(cfg)
{
	const char *name = (*cfg)["server.host"].c_str();
	const char *port = (*cfg)["server.port"].c_str();

	Callback<void(const char *)> testcbvoid;
	testcbvoid(0);
	Callback<int(const char *)> testcb;
	testcb.add(this, &Network::test);
	LOG(debug, "{}: {}", __PRETTY_FUNCTION__, testcb("www"));
	testcb.remove(this, &Network::test);
	LOG(debug, "{}: {}", __PRETTY_FUNCTION__, testcb("www"));

	// Local server name resolution
	uv_getaddrinfo_t *reqAddrInfo = new uv_getaddrinfo_t;
	reqAddrInfo->data = this;
	reqs.addRequest((uv_req_t *)reqAddrInfo);
	int err = uv_getaddrinfo(loop, reqAddrInfo, getServerInfo,
			name, port, 0);
	if (err)
		throw std::runtime_error(uv_strerror(err));
}

int Network::test(const char *str)
{
	LOG(debug, "{}: {}", __PRETTY_FUNCTION__, str);
	return 39;
}

Network::~Network()
{
	while (clients.size())
		delete clients.front();
	if (server) {
		uv_close((uv_handle_t *)server, 0);
		delete server;
		server = 0;
	}
}

void Network::addClient(NetworkClient *client)
{
	clients_mutex.lock();
	clients.push_back(client);
	clients_mutex.unlock();
}

void Network::removeClient(NetworkClient *client)
{
	clients_mutex.lock();
	clients.remove(client);
	clients_mutex.unlock();
}

std::string Network::getString(const struct sockaddr *addr)
{
	if (addr->sa_family == AF_INET) {
		char ip[INET_ADDRSTRLEN];
		int len = INET_ADDRSTRLEN;
		struct sockaddr_in *pa = (struct sockaddr_in *)addr;
		int err = uv_ip4_name(pa, ip, len);
		if (err)
			throw std::runtime_error(uv_strerror(err));
		int port = ntohs(pa->sin_port);
		return std::string(ip) + ":" + std::to_string(port);
	}
	if (addr->sa_family == AF_INET6) {
		char ip[INET6_ADDRSTRLEN];
		int len = INET6_ADDRSTRLEN;
		struct sockaddr_in6 *pa = (struct sockaddr_in6 *)addr;
		int err = uv_ip6_name(pa, ip, len);
		if (err)
			throw std::runtime_error(uv_strerror(err));
		int port = ntohs(pa->sin6_port);
		return "[" + std::string(ip) + "]:" + std::to_string(port);
	}
	throw std::runtime_error("Unsupported address family");
}

void Network::getServerInfo(uv_getaddrinfo_t *req,
		int status, struct addrinfo *res)
{
	Network *n = static_cast<Network *>(req->data);
	n->reqs.removeRequest((uv_req_t *)req);
	if (status != 0) {
		delete req;
		throw std::runtime_error(uv_strerror(status));
	}

	// Local listening server
	uv_tcp_t *server = new uv_tcp_t;
	server->data = n;

	struct addrinfo *pa;
	for (pa = res; pa != 0; pa = pa->ai_next) {
		int err = 0;
		if ((err = uv_tcp_init_ex(req->loop, server, pa->ai_family))) {
			LOG(warn, "{}", uv_strerror(err));
			continue;
		}

		// Retrieve fileno for setsockopt
		int sfd = 0, flag = 1;
		if ((err = uv_fileno((uv_handle_t *)server, &sfd)))
			LOG(warn, "{}", uv_strerror(err));
		else if ((err = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
				(const char *)&flag, sizeof(int))))
			LOG(warn, "{}", strerror(err));

		if ((err = uv_tcp_bind(server, pa->ai_addr, 0))) {
			LOG(warn, "{}", uv_strerror(err));
			uv_close((uv_handle_t *)server, 0);
			continue;
		}

		if ((err = uv_listen((uv_stream_t *)server, 16,
				serverAccept))) {
			LOG(warn, "{}", uv_strerror(err));
			uv_close((uv_handle_t *)server, 0);
			continue;
		}
		break;
	}

	if (pa) {
		try {
			LOG(info, "Listening at {}", getString(pa->ai_addr));
		} catch (std::exception &e) {
			LOG(warn, "{}", e.what());
		}
		n->server = server;
	}

	uv_freeaddrinfo(req->addrinfo);
	delete req;

	if (!pa) {
		delete server;
		throw std::runtime_error("No usable local server address");
	}
}

void Network::serverAccept(uv_stream_t *server, int status)
{
	int err = status;
	if (err)
		throw std::runtime_error(uv_strerror(err));

	Network *n = static_cast<Network *>(server->data);
	NetworkClient *nc = new NetworkClient(n);
	try {
		nc->accept(server);
		//nc->connectTo(n->remote_name.c_str(), n->remote_port.c_str());
	} catch (std::exception &e) {
		delete nc;
		throw e;
	}
	LOG(debug, "{} clients connected", n->clients.size());
}
