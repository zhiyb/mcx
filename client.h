#pragma once

#include <unistd.h>
#include <uv.h>
#include <vector>
#include "parsers.h"
#include "buffer.h"
#include "mutex.h"
#include "parser/parser.h"

class Config;
class NetworkClient;

class Client
{
public:
	Client();
	~Client();

	Config &config() const;

	// From NetworkClient thread
	void init(NetworkClient *nc, uv_loop_t *loop);
	bool shutdown();
	void read(std::vector<char> *buf);

	// From Client thread
	void threadShutdown();
	void write(std::vector<char> *buf);

private:
	// From NetworkClient thread
	void async();

	static void async(uv_async_t *handle);

	// From Client thread
	void threadInit();
	void threadDeinit();
	void threadAsync();

	void thread();
	static void thread(void *arg);
	static void threadAsync(uv_async_t *handle);

	enum thread_status_t {Uninitialised, Initialising, Running,
		Closing, Shutdown};

	NetworkClient *nc = 0;
	Parser *parser = 0;
	Parsers parsers;
	struct {
		Buffer<char> buf, threadBuf;
		Mutex mtx;
		uv_async_t async;
	} rd, wr;
	size_t bufNum = 0;
	uv_thread_t tid;
	uv_loop_t threadLoop;
	volatile bool _shutdown = false;
	volatile enum thread_status_t threadStatus = Uninitialised;
};
