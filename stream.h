#pragma once

#include <vector>
#include <functional>
#include <unistd.h>
#include <uv.h>
#include "buffer.h"
#include "callback.h"

class Stream
{
public:
	Stream(uv_stream_t *stream, size_t bufNum = 0);
	~Stream();

	bool shutdown();

	uv_stream_t *stream() const {return _stream;}
	size_t bufNum() const {return _bufNum;}
	void bufNum(size_t bufNum) {_bufNum = bufNum;}

	Callback<bool(void *)> &readCallbacks() {return _readCallbacks;}

private:
	void read();
	void readCallback();

	void remoteAlloc(size_t suggested_size, uv_buf_t *buf);
	void remoteRead(ssize_t nread, const uv_buf_t *buf);
	void remoteWrite(std::vector<char> *buf);
	void remoteWrite(uv_write_t *req);
	void remoteClose();

	// Callbacks
	static void alloc(uv_handle_t *handle,
			size_t suggested_size, uv_buf_t *buf);
	static void read(uv_stream_t *stream,
			ssize_t nread, const uv_buf_t *buf);
	static void write(uv_write_t *req, int status);
	static void close(uv_handle_t *handle);

	Callback<bool(void *)> _readCallbacks;
	Buffer<char> _rdbuf, _wrbuf;
	uv_stream_t *_stream = 0;
	size_t _bufNum = 0;
};
