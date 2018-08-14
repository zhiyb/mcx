#pragma once

#include <unistd.h>
#include <uv.h>
#include "parser/parser.h"

class Client;

class Parsers
{
public:
	Parsers();
	~Parsers();

	void setClient(Client *c) {this->c = c;}
	bool shutdown();
	void init(uv_loop_t *loop);
	bool identify(const uv_buf_t &buf);
	void read(const uv_buf_t &buf);

private:
	Client *c;
};
