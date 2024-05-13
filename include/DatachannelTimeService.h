#ifndef DATACHANNELTIMESERVICE_H
#define DATACHANNELTIMESERVICE_H

#include "Datachannels.h"
#include "TimeService.h"


class DatachannelTimer : public datachannels::Timer
{
public:
	DatachannelTimer(const ::Timer::shared& timer) : timer(timer) {};
	
	void Cancel() override
	{
		timer->Cancel();	
	}
	
	void Again(const std::chrono::milliseconds& ms) override
	{
		timer->Again(ms);
	}
	
	std::chrono::milliseconds GetRepeat() const override
	{
		return timer->GetRepeat();
	}
	
	bool IsScheduled() const override
	{
		return timer->IsScheduled();
	}
private:

	::Timer::shared timer;
};
	
class DatachannelTimeService : public datachannels::TimeService
{
public:
	DatachannelTimeService(::TimeService& timeService) : timeService(timeService) {};

	const std::chrono::milliseconds GetNow() const override
	{
		return timeService.GetNow();
	}
	
	datachannels::Timer::shared CreateTimer(std::function<void(std::chrono::milliseconds)> callback) override
	{
		return std::make_shared<DatachannelTimer>(timeService.CreateTimer(callback));
	}
	
	datachannels::Timer::shared CreateTimer(const std::chrono::milliseconds& ms, std::function<void(std::chrono::milliseconds)> timeout) override
	{
		return std::make_shared<DatachannelTimer>(timeService.CreateTimer(ms, timeout));
	}
	
	datachannels::Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, std::function<void(std::chrono::milliseconds)> timeout) override
	{
		return std::make_shared<DatachannelTimer>(timeService.CreateTimer(ms, repeat, timeout));
	}
	
	void Async(const std::function<void(std::chrono::milliseconds)>& func) override
	{
		timeService.Async(func);
	}

	void Sync(const std::function<void(std::chrono::milliseconds)>& func) override
	{
		timeService.Async(func);
	}
private:
	::TimeService& timeService;
};


#endif
