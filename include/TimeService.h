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

#endif /* TIMESERVICE_H */

