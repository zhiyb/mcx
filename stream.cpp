#include "stream.h"
#include "logging.h"

Stream::Stream(uv_stream_t *stream, size_t bufNum):
	_stream(stream), _bufNum(bufNum)
{
	read();
}

Stream::~Stream()
{
	if (_stream) {
		LOG(error, "Destructed with stream active");
		uv_close((uv_handle_t *)_stream, 0);
		delete _stream;
	}
}

bool Stream::shutdown()
{
	// TODO
	LOG(debug, "{}", __PRETTY_FUNCTION__);
	uv_read_stop(_stream);
	return true;
}

void Stream::read()
{
	// Buffer limit
	if (_bufNum && _rdbuf.size() >= _bufNum)
		return;
}
