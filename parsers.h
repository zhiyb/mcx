#pragma once

#include <vector>
#include <unistd.h>
#include <uv.h>

class Client;
class Parser;

class Parsers
{
public:
	~Parsers();

	void init(Client *c);
	bool shutdown();
	Parser *identify(void *buf, size_t len);

private:
	std::vector<Parser *> parsers;
};
