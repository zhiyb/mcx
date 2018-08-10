#include <stdexcept>
#include <iostream>
#include "networkrequests.h"
#include "logging.h"

NetworkRequests::~NetworkRequests()
{
	if (reqs.size())
		LOG(error, "Destructed with {} pending requests",
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
	for (auto *req: reqs) {
		int err;
		switch (req->type) {
		case UV_FS:
		case UV_GETADDRINFO:
		case UV_GETNAMEINFO:
		case UV_WORK:
			if ((err = uv_cancel(req)))
				LOG(warn, "{}", uv_strerror(err));
		default:
			break;
		}
	}
	mutex.unlock();
}

void NetworkRequests::print()
{
	mutex.lock();
	for (auto req: reqs)
		LOG(debug, "{}: {}", __PRETTY_FUNCTION__, req->type);
	mutex.unlock();
}
