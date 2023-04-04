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
	virtual Timer::shared CreateTimer(std::function<void(std::chrono::milliseconds)> callback) override
	{
		assert(false); // Not used
		return nullptr;
	}
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, std::function<void(std::chrono::milliseconds)> timeout) override
	{
		assert(false); // Not used
		return nullptr;
	}
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms,
										const std::chrono::milliseconds& repeat,
										std::function<void(std::chrono::milliseconds)> timeout) override
	{
		assert(false); // Not used
		return nullptr;
	};

	virtual std::future<void> Async(std::function<void(std::chrono::milliseconds)> func) override
	{
		return std::async(std::launch::deferred, [=](){ func(std::chrono::milliseconds()); });
	}
};

#endif