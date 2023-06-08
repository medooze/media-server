#ifndef TESTCOMMON_H
#define TESTCOMMON_H

#include <include/gtest/gtest.h>
#include "TimeService.h"

class TestTimeService : public TimeService
{
public:
	virtual const std::chrono::milliseconds GetNow() const override
	{
		assert(false); // Not used
		return std::chrono::milliseconds();
	}
	virtual Timer::shared CreateTimer(const std::function<void(std::chrono::milliseconds)>& callback) override
	{
		assert(false); // Not used
		return nullptr;
	}
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::function<void(std::chrono::milliseconds)>& timeout) override
	{
		assert(false); // Not used
		return nullptr;
	}
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, const std::function<void(std::chrono::milliseconds)>& timeout) override
	{
		assert(false); // Not used
		return nullptr;
	};


	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func) override
	{
		func(std::chrono::milliseconds());
	}

	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback) override
	{
		func(std::chrono::milliseconds());
	}

	virtual std::future<void> Future(const std::function<void(std::chrono::milliseconds)>& func) override
	{
		func(std::chrono::milliseconds());
		return std::async(std::launch::deferred, []() {});
	}
};

#endif