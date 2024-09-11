#ifndef TIMESERVICE_H
#define TIMESERVICE_H
#include <stdint.h>
#include <chrono>
#include <memory>
#include <string>
#include <functional>
#include <future>

class Timer
{
public:
	using shared = std::shared_ptr<Timer>;
public:
	virtual ~Timer() = default;
	virtual void Cancel() = 0;
	virtual void Again(const std::chrono::milliseconds& ms) = 0;
	virtual bool IsScheduled() const = 0;
	virtual void Repeat(const std::chrono::milliseconds& repeat) = 0;
	virtual void Reschedule(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat) = 0;
	virtual std::chrono::milliseconds GetNextTick()	const = 0;
	virtual std::chrono::milliseconds GetRepeat() const = 0;
	void SetName(const std::string& name) { this->name = name; }
	std::string GetName() const  { return name; }
private:
	std::string name;
};
	
class TimeService
{
public:
	virtual ~TimeService() = default;
	virtual const std::chrono::milliseconds GetNow() const = 0;
	virtual Timer::shared CreateTimer(const std::function<void(std::chrono::milliseconds)>& callback) = 0;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::function<void(std::chrono::milliseconds)>& timeout) = 0;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, const std::function<void(std::chrono::milliseconds)>& timeout) = 0;
	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func) = 0;
	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback) = 0;
	virtual std::future<void> Future(const std::function<void(std::chrono::milliseconds)>& func) = 0;
	inline void Sync(const std::function<void(std::chrono::milliseconds)>& func) 
	{
		//Run async and wait for future
		Future(func).wait();
	}
};


template <typename T>
class TimeServiceWrapper : public std::enable_shared_from_this<T>
{
public:
	TimeServiceWrapper(TimeService& timeService) : timeService(timeService)
	{
	}
	
	template <typename Func>
	void AsyncSafe(Func&& func)
	{
		timeService.Async([selfWeak = TimeServiceWrapper<T>::weak_from_this(), func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}
	
	template <typename Func>
	auto CreateTimerSafe(Func&& func)
	{
		return timeService.CreateTimer([selfWeak = TimeServiceWrapper<T>::weak_from_this(), func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}
	
	template <typename Func>
	auto CreateTimerSafe(const std::chrono::milliseconds& ms, Func&& func)
	{
		return timeService.CreateTimer(ms, [selfWeak = TimeServiceWrapper<T>::weak_from_this(), func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}
	
	template <typename Func>
	auto FutureSafe(Func&& func)
	{
		return timeService.Future([selfWeak = TimeServiceWrapper<T>::weak_from_this(), func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}
	
	template <typename... ARGS>
	static std::shared_ptr<T> Create(ARGS&&... args)
	{
		return std::shared_ptr<T>(new T(std::forward<ARGS>(args)...));
	}

private:
	TimeService& timeService;
};


#endif /* TIMESERVICE_H */

