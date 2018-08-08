#include <stdexcept>
#include <iostream>
#include "networkrequests.h"
#include "logging.h"

NetworkRequests::~NetworkRequests()
{
	if (reqs.size())
		logger->error("Destructed with {} pending requests",
				reqs.size());
}

void NetworkRequests::addRequest(uv_req_t *req)
{
	if (!req)
		return;
	mutex.lock();
	reqs.push_back(req);
	mutex.unlock();
}

void NetworkRequests::removeRequest(uv_req_t *req)
{
	if (!req)
		return;
	mutex.lock();
	reqs.remove(req);
	mutex.unlock();
}
