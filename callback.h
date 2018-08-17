#pragma once

#include <algorithm>
#include <functional>
#include <vector>

template<class>
class Callback;

template<class R, class... Args>
class Callback<R(Args...)>
{
public:
	~Callback()
	{
		for (auto *p: cbc)
			delete p;
	}

	template<class T>
	Callback &add(T *obj, R(T::* func)(Args...))
	{
		for (auto *p: cbc)
			if (p->o == obj &&
				static_cast<_Callback<T> *>(p)->f == func)
					return *this;
		cbc.push_back(new _Callback<T>(obj, func));
		return *this;
	}

	template<class T>
	Callback &remove(T *obj, R(T::* func)(Args...))
	{
		for (auto it = cbc.begin(); it != cbc.end(); it++) {
			auto *p = *it;
			if (p->o == obj &&
				static_cast<_Callback<T> *>(p)->f == func) {
				delete p;
				cbc.erase(it);
				break;
			}
		}
		return *this;
	}

	Callback &operator+=(std::function<R(Args...)> cb)
	{
		cbf.push_back(cb);
		return *this;
	}

	Callback &operator-=(std::function<R(Args...)> cb)
	{
		cbf.remove(cb);
		return *this;
	}

	template <class T = R>
	std::enable_if_t<std::is_void<T>::value, T>
	operator()(Args... args)
	{
		for (auto f: cbf)
			f(args...);
		for (auto p: cbc)
			(*p)(args...);
	}

	template <class T = R>
	std::enable_if_t<not std::is_void<T>::value, T>
	operator()(Args... args)
	{
		T ret = 0;
		for (auto f: cbf)
			ret |= f(args...);
		for (auto p: cbc)
			ret |= (*p)(args...);
		return ret;
	}

private:
	class _CallbackBase
	{
	public:
		_CallbackBase(void *obj): o(obj) {}
		virtual ~_CallbackBase() {}
		virtual R operator()(Args... args) = 0;

		void *o;
	};

	template<class T>
	class _Callback: public _CallbackBase
	{
	public:
		_Callback(T *o, R(T::* f)(Args...)):
			_CallbackBase((void *)o), f(f) {}
		virtual ~_Callback() {}
		virtual R operator()(Args... args)
		{return (static_cast<T *>(_CallbackBase::o)->*f)(args...);}

		R (T::* f)(Args...);
	};

	std::vector<std::function<R(Args...)>> cbf;
	std::vector<_CallbackBase *> cbc;
};
