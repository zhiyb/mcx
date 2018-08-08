#pragma once

#include <uv.h>

class Mutex
{
public:
	Mutex();
	~Mutex() {uv_mutex_destroy(&mutex);}

	void lock() {uv_mutex_lock(&mutex);}
	int tryLock() {return uv_mutex_trylock(&mutex);}
	void unlock() {uv_mutex_unlock(&mutex);}

private:
	uv_mutex_t mutex;
};
