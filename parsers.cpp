#include <stdexcept>
#include "parsers.h"

#include "parser/parser.h"
#include "parser/mcx.h"

void Parsers::init(Client *c)
{
	parsers.push_back(new ParserMCX(c));
}

Parsers::~Parsers()
{
	for (auto *p: parsers)
		delete p;
}

bool Parsers::shutdown()
{
	bool shutdown = true;
	for (auto *p: parsers)
		if (p && !p->shutdown())
			shutdown = false;
	if (shutdown) {
		for (auto *p: parsers)
			delete p;
		parsers.clear();
	}
	return shutdown;
}

Parser *Parsers::identify(void *buf, size_t len)
{
	bool matching = false;
	for (auto *&p: parsers) {
		if (!p)
			continue;

		bool match;
		try {
			match = p->identify(buf, len);
		} catch (std::exception &e) {
			delete p;
			p = 0;
			continue;
		}

		matching = true;
		if (match) {
			auto *parser = p;
			for (auto *pp: parsers)
				if (pp != p)
					delete pp;
			parsers.clear();
			return parser;
		}
	}
	if (!matching)
		throw std::runtime_error("No matching parser");
	return 0;
}
