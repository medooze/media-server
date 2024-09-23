#ifndef TIMESERVICE_H
#define TIMESERVICE_H
#include <stdint.h>
#include <chrono>
#include <memory>
#include <string>
#include <functional>
#include <future>
#include <cassert>
#include <utility>

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
	// @todo Rename now to force compile errors if used, then rename back
	virtual ~TimeService() = default;
	virtual const std::chrono::milliseconds GetNow() const = 0;
	virtual Timer::shared CreateTimerUnsafe(const std::function<void(std::chrono::milliseconds)>& callback) = 0;
	virtual Timer::shared CreateTimerUnsafe(const std::chrono::milliseconds& ms, const std::function<void(std::chrono::milliseconds)>& timeout) = 0;
	virtual Timer::shared CreateTimerUnsafe(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, const std::function<void(std::chrono::milliseconds)>& timeout) = 0;
	virtual void AsyncUnsafe(const std::function<void(std::chrono::milliseconds)>& func) = 0;
	virtual void AsyncUnsafe(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback) = 0;
	virtual std::future<void> FutureUnsafe(const std::function<void(std::chrono::milliseconds)>& func) = 0;
	inline void SyncUnsafe(const std::function<void(std::chrono::milliseconds)>& func) 
	{
		//Run async and wait for future
		FutureUnsafe(func).wait();
	}
};


template <typename T>
class TimeServiceWrapper : public std::enable_shared_from_this<T>
{
public:

	template <typename... ARGS>
	static std::shared_ptr<T> Create(ARGS&&... args)
	{
		auto obj = std::shared_ptr<T>(new T(std::forward<ARGS>(args)...));
		obj->OnCreated();
		return obj;
	}


	TimeServiceWrapper(TimeService& timeService) : timeService(timeService)
	{
	}
	
	virtual ~TimeServiceWrapper() = default;
	
	// Override this to call functions of this base class as they cannot be called in constructors
	virtual void OnCreated() {}

	template <typename Func>
	inline void Sync(Func&& func)
	{
		timeService.SyncUnsafe(std::forward<Func>(func));
	}
	
	template <typename Func>
	void AsyncSafe(Func&& func)
	{
		auto selfWeak = TimeServiceWrapper<T>::weak_from_this();
		// If following assert failed, the function might be called in constructor. See OnCreated() description.
		assert(!selfWeak.expired());
		
		timeService.AsyncUnsafe([selfWeak, func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}

	template <typename Func, typename Callback>
	void AsyncSafe(Func&& func, Callback&& callback)
	{
		auto selfWeak = TimeServiceWrapper<T>::weak_from_this();
		// If following assert failed, the function might be called in constructor. See OnCreated() description.
		assert(!selfWeak.expired());
		
		timeService.AsyncUnsafe([selfWeak, func = std::forward<Func>(func)](std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;

			func(self, now);
		}, std::forward<Callback>(callback));
	}
	
	template <typename Func>
	auto CreateTimerSafe(Func&& func)
	{
		auto selfWeak = TimeServiceWrapper<T>::weak_from_this();
		// If following assert failed, the function might be called in constructor. See OnCreated() description.
		assert(!selfWeak.expired());
		
		return timeService.CreateTimerUnsafe([selfWeak, func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}
	
	template <typename Func>
	auto CreateTimerSafe(const std::chrono::milliseconds& ms, Func&& func)
	{
		auto selfWeak = TimeServiceWrapper<T>::weak_from_this();
		// If following assert failed, the function might be called in constructor. See OnCreated() description.
		assert(!selfWeak.expired());
		
		return timeService.CreateTimerUnsafe(ms, [selfWeak, func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}
	
	template <typename Func>
	auto CreateTimerSafe(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, Func&& func)
	{
		auto selfWeak = TimeServiceWrapper<T>::weak_from_this();
		// If following assert failed, the function might be called in constructor. See OnCreated() description.
		assert(!selfWeak.expired());
		
		return timeService.CreateTimerUnsafe(ms, repeat, [selfWeak, func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}

	template <typename Func>
	auto FutureSafe(Func&& func)
	{
		auto selfWeak = TimeServiceWrapper<T>::weak_from_this();
		// If following assert failed, the function might be called in constructor. See OnCreated() description.
		assert(!selfWeak.expired());
		
		return timeService.FutureUnsafe([selfWeak, func = std::forward<Func>(func)] (std::chrono::milliseconds now) mutable {
			auto self = selfWeak.lock();
			if (!self) return;
			
			func(self, now);
		});
	}
	
	TimeService& GetTimeService()	{ return timeService; }

private:
	TimeService& timeService;
};


#endif /* TIMESERVICE_H */

