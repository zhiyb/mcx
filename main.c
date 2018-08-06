#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <uv.h>
#include "network.h"

int main(int argc, char *argv[])
{
	if (argc != 5)
		return 1;

	uv_loop_t *loop = malloc(sizeof(uv_loop_t));
	uv_loop_init(loop);

	struct network_t *n = network_init(loop,
			argv[1], argv[2], argv[3], argv[4]);
	uv_run(loop, UV_RUN_DEFAULT);

	network_close(n);
	uv_loop_close(loop);
	free(loop);
	return 0;
}
