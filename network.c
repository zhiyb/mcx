#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include "network.h"

struct network_client_t
{
	struct network_client_t *next;
	uv_tcp_t client, remote;
	uv_connect_t connect;
};

struct network_t
{
	const char *remote_name, *remote_port;
	uv_tcp_t server;
	struct network_client_t *clients;
	uv_mutex_t clients_mutex;
};

static void network_accept(uv_stream_t *server, int status);
static void network_remote(uv_connect_t *req, int status);

struct network_t *network_init(uv_loop_t *loop,
		const char *name, const char *port,
		const char *remote_name, const char *remote_port)
{
	struct network_t *n = calloc(1, sizeof(struct network_t));
	if (!n)
		return 0;
	n->remote_name = remote_name;
	n->remote_port = remote_port;

	int err = 0;
	if ((err = uv_mutex_init(&n->clients_mutex))) {
		fprintf(stderr, "%s:%d Error: %s\n",
				__func__, __LINE__, uv_strerror(err));
		goto err;
	}

	// Local server name resolution
	struct addrinfo *psi = 0;
	fprintf(stderr, "%s:%d Listening at %s:%s\n",
			__func__, __LINE__, name, port);
	if ((err = getaddrinfo(name, port, NULL, &psi))) {
		fprintf(stderr, "%s:%d Error: %s\n",
				__func__, __LINE__, gai_strerror(err));
		goto err;
	}

	// Setup listening server
	struct addrinfo *pa;
	for (pa = psi; pa != NULL; pa = pa->ai_next) {
		if ((err = uv_tcp_init_ex(loop, &n->server, pa->ai_family))) {
			fprintf(stderr, "%s:%d Warning: %s\n",
					__func__, __LINE__, uv_strerror(err));
			continue;
		}
		n->server.data = n;

		// Retrieve fileno for setsockopt
		int sfd;
		if ((err = uv_fileno((uv_handle_t *)&n->server, &sfd))) {
			fprintf(stderr, "%s:%d Warning: %s\n",
					__func__, __LINE__, uv_strerror(err));
			uv_close((uv_handle_t *)&n->server, NULL);
			continue;
		}

		int flag = 1;
		if ((err = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
				(const char *)&flag, sizeof(int))))
			fprintf(stderr, "%s:%d Warning: %s\n",
					__func__, __LINE__, strerror(err));

		if ((err = uv_tcp_bind(&n->server, pa->ai_addr, 0))) {
			fprintf(stderr, "%s:%d Warning: %s\n",
					__func__, __LINE__, uv_strerror(err));
			uv_close((uv_handle_t *)&n->server, NULL);
			continue;
		}

		if ((err = uv_listen((uv_stream_t *)&n->server, 16,
				network_accept))) {
			fprintf(stderr, "%s:%d Warning: %s\n",
					__func__, __LINE__, uv_strerror(err));
			uv_close((uv_handle_t *)&n->server, NULL);
			continue;
		}
		break;
	}

	freeaddrinfo(psi);
	if (!pa) {
		fprintf(stderr, "%s:%d Error: No server connection\n",
				__func__, __LINE__);
		goto err;
	}
	return n;

err:	network_close(n);
	return 0;
}

void network_close(struct network_t *n)
{
	if (!n)
		return;
	uv_close((uv_handle_t *)&n->server, NULL);
	struct network_client_t *pnc = n->clients;
	while (pnc) {
		void *p = pnc;
		pnc = pnc->next;
		uv_close((uv_handle_t *)&pnc->remote, NULL);
		uv_close((uv_handle_t *)&pnc->client, NULL);
		free(p);
	}
	uv_mutex_destroy(&n->clients_mutex);
	free(n);
}

static void network_accept(uv_stream_t *server, int status)
{
	int err = status;
	if (err) {
		fprintf(stderr, "%s:%d Error: %s\n",
				__func__, __LINE__, uv_strerror(err));
		return;
	}

	struct network_t *n = server->data;
	struct network_client_t *pnc = calloc(1,
			sizeof(struct network_client_t));
	if ((err = uv_tcp_init(server->loop, &pnc->client))) {
		fprintf(stderr, "%s:%d Error: %s\n",
				__func__, __LINE__, uv_strerror(err));
		goto err;
	}
	pnc->client.data = n;

	if ((err = uv_accept(server, (uv_stream_t *)&pnc->client))) {
		fprintf(stderr, "%s:%d Error: %s\n",
				__func__, __LINE__, uv_strerror(err));
		goto err;
	}

	// Print peer name
	struct sockaddr_storage addr;
	int len = sizeof(addr);
	if (!(err = uv_tcp_getpeername(&pnc->client,
			(struct sockaddr *)&addr, &len))) {
		if (addr.ss_family == AF_INET) {
			char ip[INET_ADDRSTRLEN];
			len = INET_ADDRSTRLEN;
			struct sockaddr_in *pa = (struct sockaddr_in *)&addr;
			if (!uv_ip4_name(pa, ip, len))
				fprintf(stderr, "%s:%d Client from %s:%d\n",
						__func__, __LINE__,
						ip, pa->sin_port);
		}
		if (addr.ss_family == AF_INET6) {
			char ip[INET6_ADDRSTRLEN];
			len = INET6_ADDRSTRLEN;
			struct sockaddr_in6 *pa = (struct sockaddr_in6 *)&addr;
			if (!uv_ip6_name(pa, ip, len))
				fprintf(stderr, "%s:%d Client from %s:%d\n",
						__func__, __LINE__,
						ip, pa->sin6_port);
		}
	}

	if ((err = uv_tcp_nodelay(&pnc->client, 1)))
		fprintf(stderr, "%s:%d Warning: %s\n",
				__func__, __LINE__, uv_strerror(err));

	// Remote server name resolution
	fprintf(stderr, "%s:%d Connecting to %s:%s\n", __func__, __LINE__,
			n->remote_name, n->remote_port);
	struct addrinfo *pri = 0;
	if ((err = getaddrinfo(n->remote_name, n->remote_port,
			NULL, &pri))) {
		fprintf(stderr, "%s:%d Error: %s\n",
				__func__, __LINE__, gai_strerror(err));
		goto err;
	}

	// Connect to remote server
	struct addrinfo *pa;
	for (pa = pri; pa != NULL; pa = pa->ai_next) {
		if ((err = uv_tcp_init(server->loop, &pnc->remote))) {
			fprintf(stderr, "%s:%d Warning: %s\n",
					__func__, __LINE__, uv_strerror(err));
			continue;
		}
		pnc->remote.data = n;

		if ((err = uv_tcp_connect(&pnc->connect, &pnc->remote,
				pa->ai_addr, network_remote))) {
			fprintf(stderr, "%s:%d Warning: %s\n",
					__func__, __LINE__, uv_strerror(err));
			uv_close((uv_handle_t *)&pnc->remote, NULL);
			continue;
		}
		break;
	}
	freeaddrinfo(pri);
	if (!pa) {
		fprintf(stderr, "%s:%d Error: No remote connection\n",
				__func__, __LINE__);
		goto err;
	}

	uv_mutex_lock(&n->clients_mutex);
	pnc->next = n->clients;
	n->clients = pnc;
	uv_mutex_unlock(&n->clients_mutex);
	return;

err:	uv_close((uv_handle_t *)&pnc->remote, NULL);
	uv_close((uv_handle_t *)&pnc->client, NULL);
	free(pnc);
}

static void network_remote(uv_connect_t *req, int status)
{
	int err = status;
	if (err) {
		fprintf(stderr, "%s:%d Error: %s\n",
				__func__, __LINE__, uv_strerror(err));
		return;
	}

	fprintf(stderr, "%s:%d www\n", __func__, __LINE__);
}
