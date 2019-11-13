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
	virtual std::chrono::milliseconds GetNextTick()	const = 0;
	virtual std::chrono::milliseconds GetRepeat() const = 0;
};
	
class TimeService
{
public:
	virtual ~TimeService() = default;
	virtual const std::chrono::milliseconds GetNow() const = 0;
	virtual Timer::shared CreateTimer(std::function<void(std::chrono::milliseconds)> callback) = 0;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, std::function<void(std::chrono::milliseconds)> timeout) = 0;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, std::function<void(std::chrono::milliseconds)> timeout) = 0;
	virtual std::future<void> Async(std::function<void(std::chrono::milliseconds)> func) = 0;
	inline void Sync(std::function<void(std::chrono::milliseconds)> func) 
	{
		//Run async and wait for future
		Async(func).wait();
	}
};

#endif /* TIMESERVICE_H */

