#include <stdexcept>
#include "mutex.h"

Mutex::Mutex()
{
	int err = uv_mutex_init(&mutex);
	if (err)
		throw std::runtime_error(uv_strerror(err));
}
