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

void NetworkRequests::cancelAll()
{
	mutex.lock();
	for (auto req: reqs) {
		int err = uv_cancel(req);
		if (err)
			logger->warn("{}", uv_strerror(err));
	}
	mutex.unlock();
}

void NetworkRequests::print()
{
	mutex.lock();
	for (auto req: reqs)
		logger->debug("{}: {}", __PRETTY_FUNCTION__, req->type);
	mutex.unlock();
}
