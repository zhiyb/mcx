#pragma once

#include <functional>
#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> logger;

#define LOG(level, msg, ...)	logger->level("{}:{} " msg, \
		__FILE__, __LINE__, ##__VA_ARGS__)
