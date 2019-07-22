#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <uv.h>
#include "logging.h"
#include "config.h"
#include "network.h"

std::shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("console");

int main(int argc, char *argv[])
{
	//logger->set_pattern("[%Y-%m-%d %T t%t] [%l] %v");
	logger->set_pattern("[%T %t/%l]: %v");
	logger->set_level(spdlog::level::debug);

	if (argc != 5) {
		LOG(error, "Need {} arguments", 4);
		return 1;
	}

	Config cfg;
	cfg["server.host"] = argv[1];
	cfg["server.port"] = argv[2];
	cfg["mcx.remote.host"] = argv[3];
	cfg["mcx.remote.port"] = argv[4];

	cfg["client.buffers"] = 2;
	cfg["mcx.remote.buffers"] = 2;

	uv_loop_t loop;
	uv_loop_init(&loop);

	Network *n = new Network(&loop, &cfg);
	uv_run(&loop, UV_RUN_DEFAULT);

	delete n;
	uv_loop_close(&loop);
	return 0;
}
