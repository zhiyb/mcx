#include <cstring>
#include <string>
#include <stdexcept>
#include "logging.h"
#include "networkclient.h"
#include "client.h"
#include "config.h"

Client::Client()
{
	parsers.init(this);
}

Client::~Client()
{
	if (threadStatus != Uninitialised) {
		LOG(warn, "Thread not closed");
		threadShutdown();
		threadDeinit();
		uv_thread_join(&tid);
		threadStatus = Uninitialised;
	}
	if (nc) {
		LOG(warn, "Client not closed");
		shutdown();
	}
}

void Client::init(NetworkClient *nc, uv_loop_t *loop)
{
	int err = 0;
	if ((err = uv_async_init(loop, &wr.async, async)))
		throw std::runtime_error(uv_strerror(err));
	wr.async.data = this;
	this->nc = nc;

	try {
		bufNum = std::stoi(config()["client.buffers"]);
	} catch (std::exception &e) {
		LOG(warn, "{}", e.what());
		bufNum = 0;
	}
}

Config &Client::config() const
{
	return nc->config();
}

bool Client::shutdown()
{
	if (!parsers.shutdown())
		return false;
	_shutdown = true;
	switch (threadStatus) {
	case Uninitialised:
		break;
	case Running:
		uv_async_send(&rd.async);
	case Initialising:
	case Closing:
		return false;
	case Shutdown:
		uv_thread_join(&tid);
		threadStatus = Uninitialised;
	}
	if (nc) {
		wr.mtx.lock();
		uv_close((uv_handle_t *)&wr.async, 0);
		wr.mtx.unlock();
		nc = 0;
	}
	return true;
}

void Client::read(std::vector<char> *buf)
{
	int err = 0;
	if (_shutdown)
		return;

	// Single thread identify mode
	if (threadStatus == Uninitialised) {
		rd.buf.enqueue(buf);
		uv_buf_t buf = rd.buf.combine();
		if (!(parser = parsers.identify(buf.base, buf.len)))
			// TODO: Timeout / data length limit
			return;
		threadStatus = Initialising;
		if ((err = uv_thread_create(&tid, thread, this))) {
			threadStatus = Uninitialised;
			throw std::runtime_error(uv_strerror(err));
		}
		return;
	}

	// Separate thread client mode
	rd.mtx.lock();
	rd.buf.enqueue(buf);
	bool stop = bufNum && rd.buf.size() >= bufNum;
	rd.mtx.unlock();

	if (threadStatus == Running)
		uv_async_send(&rd.async);
	if (stop)
		nc->readStop();
}

void Client::async()
{
	// Shutdown event
	if (threadStatus == Shutdown) {
		nc->close();
		return;
	}

	// Write event
	if (!wr.threadBuf)
		return;
	wr.mtx.lock();
	wr.threadBuf.swap(wr.buf);
	wr.mtx.unlock();

	wr.buf.combine();
	nc->write(wr.buf.dequeue());

	// Read restart event
	rd.mtx.lock();
	if (!nc->readActive() && (!bufNum || rd.buf.size() < bufNum))
		nc->read();
	rd.mtx.unlock();
}

void Client::async(uv_async_t *handle)
{
	Client *c = static_cast<Client *>(handle->data);
	c->async();
}

void Client::write(std::vector<char> *buf)
{
	// Single thread identify mode
	if (threadStatus == Uninitialised) {
		// TODO
		LOG(error, "Unimplemented");
		return;
	}

	wr.mtx.lock();
	wr.threadBuf.enqueue(buf);
	wr.mtx.unlock();
	uv_async_send(&wr.async);
}

void Client::threadInit()
{
	int err = 0;
	if ((err = uv_loop_init(&threadLoop)))
		goto err;
	if ((err = uv_async_init(&threadLoop, &rd.async, threadAsync)))
		goto err2;
	rd.async.data = this;
	return;
err2:	uv_loop_close(&threadLoop);
err:	throw std::runtime_error(uv_strerror(err));
}

void Client::threadDeinit()
{
	int err = 0;
	if ((err = uv_loop_close(&threadLoop)))
		throw std::runtime_error(uv_strerror(err));
}

void Client::threadShutdown()
{
	if (threadStatus != Running)
		return;
	if (parser && !parser->shutdown())
		return;
	uv_close((uv_handle_t *)&rd.async, 0);
	threadStatus = Closing;
	LOG(debug, "{}", __PRETTY_FUNCTION__);
}

void Client::thread()
{
	parser->init(this, &threadLoop);
	uv_run(&threadLoop, UV_RUN_DEFAULT);
}

void Client::thread(void *arg)
{
	Client *c = static_cast<Client *>(arg);
	if (c->_shutdown)
		goto quit;

	try {
		c->threadInit();
	} catch (std::exception &e) {
		LOG(warn, "Thread {}: Exception: {}",
				static_cast<void *>(c), e.what());
		goto quit;
	}
	c->threadStatus = Running;
	LOG(debug, "Thread {}: Running", static_cast<void *>(c));

	if (c->_shutdown)
		c->threadShutdown();
	else
		uv_async_send(&c->rd.async);

	try {
		c->thread();
	} catch (std::exception &e) {
		LOG(warn, "Thread {}: Exception: {}",
				static_cast<void *>(c), e.what());
		c->threadShutdown();
	}

	try {
		c->threadDeinit();
	} catch (std::exception &e) {
		LOG(warn, "Thread {}: Exception: {}",
				static_cast<void *>(c), e.what());
	}

quit:	c->wr.mtx.lock();
	c->threadStatus = Shutdown;
	// Notify Client to shutdown
	uv_async_send(&c->wr.async);
	c->wr.mtx.unlock();
	LOG(debug, "Thread {}: Shutdown", static_cast<void *>(c));
}

void Client::threadAsync()
{
	// Shutdown event
	if (_shutdown) {
		threadShutdown();
		return;
	}

	// Read event
	if (rd.buf.empty())
		return;
	rd.mtx.lock();
	rd.threadBuf.swap(rd.buf);
	rd.mtx.unlock();
	if (!nc->readActive())
		uv_async_send(&wr.async);

	while (!rd.threadBuf.empty()) {
		auto *p = rd.threadBuf.dequeue();
		parser->read(p);
	}
}

void Client::threadAsync(uv_async_t *handle)
{
	Client *c = static_cast<Client *>(handle->data);
	c->threadAsync();
}
