#pragma once

#include <uv.h>

struct network_t;

struct network_t *network_init(uv_loop_t *loop,
		const char *name, const char *port,
		const char *remote_name, const char *remote_port);
void network_close(struct network_t *);
