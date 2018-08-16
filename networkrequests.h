#pragma once

#include <list>
#include <uv.h>
#include "mutex.h"

class NetworkRequests
{
public:
	~NetworkRequests();
	void cancelAll();
	bool busy() {return reqs.size() != 0;}
	void print();

	void addRequest(uv_req_t *req);
	void removeRequest(uv_req_t *req);

	template <typename T>
	void addRequest(T *req) {addRequest((uv_req_t *)req);}

	template <typename T>
	void removeRequest(T *req) {removeRequest((uv_req_t *)req);}

	template <typename T>
	NetworkRequests &operator+=(T *req)
	{
		addRequest(req);
		return *this;
	}

	template <typename T>
	NetworkRequests &operator-=(T *req)
	{
		removeRequest(req);
		return *this;
	}

	operator bool() {return busy();}

private:
	Mutex mutex;
	std::list<uv_req_t *> reqs;
};
