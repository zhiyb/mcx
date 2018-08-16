#include "parser.h"
#include "../client.h"

void Parser::init(Client *c, uv_loop_t *loop)
{
	_client = c;
	_loop = loop;
	init();
}

void Parser::close()
{
	client()->threadShutdown();
}

void Parser::write(std::vector<char> *buf)
{
	client()->write(buf);
}

Config &Parser::config() const
{
	return client()->config();
}
