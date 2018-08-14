#pragma once

#include <string>
#include <uv.h>

class Parser
{
public:
	Parser() {}
	~Parser() {}

	virtual std::string name() = 0;
	virtual bool identify(uv_buf_t buf) = 0;
};
