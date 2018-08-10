#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <uv.h>
#include "logging.h"
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

	uv_loop_t loop;
	uv_loop_init(&loop);

	Network *n = new Network(&loop, argv[1], argv[2], argv[3], argv[4]);
	uv_run(&loop, UV_RUN_DEFAULT);

	delete n;
	uv_loop_close(&loop);
	return 0;
}
