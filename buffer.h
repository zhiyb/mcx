#pragma once

#include <queue>
#include <vector>
#include <uv.h>

template <typename T>
class Buffer
{
public:
	~Buffer();

	void enqueue(std::vector<T> *buf);
	std::vector<T> *dequeue();
	void swapEnqueue(std::vector<T> &buf);
	void swap(Buffer<T> &buf);

	size_t size() {return queue.size();}
	bool empty() {return !size();}
	uv_buf_t combine();

	operator bool() {return !empty();}

private:
	std::queue<std::vector<T> *> queue;
};

template <typename T>
Buffer<T>::~Buffer()
{
	while (queue.size()) {
		auto *p = queue.front();
		queue.pop();
		delete p;
	}
}

template <typename T>
void Buffer<T>::enqueue(std::vector<T> *buf)
{
	queue.push(buf);
}

template <typename T>
std::vector<T> *Buffer<T>::dequeue()
{
	if (!queue.size())
		return 0;
	auto *p = queue.front();
	queue.pop();
	return p;
}

template <typename T>
void Buffer<T>::swapEnqueue(std::vector<T> &buf)
{
	std::vector<T> *lbuf = new std::vector<T>();
	std::swap(buf, *lbuf);
	enqueue(lbuf);
}

template <typename T>
void Buffer<T>::swap(Buffer<T> &buf)
{
	std::swap(queue, buf.queue);
}

template <typename T>
uv_buf_t Buffer<T>::combine()
{
	if (queue.size() > 1) {
		auto *p = queue.front();
		queue.pop();
		while (queue.size()) {
			auto *pp = queue.front();
			queue.pop();
			p->insert(p->end(), pp->begin(), pp->end());
			delete pp;
		}
		queue.push(p);
	}
	if (!queue.size()) {
		return (uv_buf_t){0};
	} else {
		auto *p = queue.front();
		return (uv_buf_t){.base = p->data(), .len = p->size()};
	}
}
