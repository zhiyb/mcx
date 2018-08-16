#pragma once

#include <string>
#include <vector>
#include <unistd.h>
#include <uv.h>

class Client;
class Config;

class Parser
{
public:
	Parser(Client *c): _client(c) {}
	virtual ~Parser() {}

	virtual std::string name() = 0;
	virtual bool shutdown() {return true;}
	virtual bool identify(void *buf, size_t len) = 0;

	// Functions called only after parser matched

	void init(Client *c, uv_loop_t *loop);
	virtual void init() {}
	// Callee manages the memory
	virtual void read(std::vector<char> *buf) = 0;

protected:
	Client *client() const {return _client;}
	Config &config() const;
	uv_loop_t *loop() {return _loop;}

	void close();
	void write(std::vector<char> *buf);

private:
	Client *_client;
	uv_loop_t *_loop;
};
