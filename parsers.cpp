#include "parsers.h"

Parsers::Parsers()
{
}

Parsers::~Parsers()
{
}

bool Parsers::shutdown()
{
	return true;
}

void Parsers::init(uv_loop_t *loop)
{
}

bool Parsers::identify(const uv_buf_t &buf)
{
	return true;
}

void Parsers::read(const uv_buf_t &buf)
{
}
