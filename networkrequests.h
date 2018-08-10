#pragma once

#include <list>
#include <uv.h>
#include "mutex.h"

class NetworkRequests
{
public:
	~NetworkRequests();
	void addRequest(uv_req_t *req);
	void removeRequest(uv_req_t *req);
	void cancelAll();
	bool busy() {return reqs.size() != 0;}
	void print();

private:
	Mutex mutex;
	std::list<uv_req_t *> reqs;
};
