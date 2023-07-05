#ifndef TESTCOMMON_H
#define TESTCOMMON_H

#include <include/gtest/gtest.h>
#include "TimeService.h"
#include <memory>
#include <queue>


class TestTimeService : public TimeService
{
public:

	class TimerImpl : public Timer, public std::enable_shared_from_this<TimerImpl>
	{
	public:
		using shared = std::shared_ptr<TimerImpl>;
	
		TimerImpl(TestTimeService& timeService, std::function<void(std::chrono::milliseconds)> callback) : 
			loop(timeService),
			next(0),
			callback(callback)
		{
		}
	
		virtual void Cancel() override
		{
			assert(false); // Not used
		}
		
		virtual void Again(const std::chrono::milliseconds& ms)
		{
			next = loop.GetNow() + ms;
			
			loop.pendingTimers.emplace_back(next, shared_from_this());
		}
		
		virtual void Repeat(const std::chrono::milliseconds& repeat)
		{
			assert(false); // Not used
		}
		
		virtual void Reschedule(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat)
		{
			assert(false); // Not used
		}
		

		virtual bool IsScheduled()			const override { return next.count();	}
		virtual std::chrono::milliseconds GetNextTick()	const override { return next;		}
		virtual std::chrono::milliseconds GetRepeat()	const override 
		{
			assert(false); // Not used 
			return std::chrono::milliseconds{0}; 
		}
		
		std::function<void(std::chrono::milliseconds)> callback;
		std::chrono::milliseconds next;
		
	private:
		TestTimeService& loop;
	};

	/**
	 * Set current time for testing. It will triggered scheduled timers
	 * that has expired at the time.
	 */
	inline void SetNow(std::chrono::milliseconds time)
	{
		// Trigger the scheduled timer by order
		while (!timers.empty() && timers.top().first <= time)
		{
			now = timers.top().first;
			timers.top().second->callback(now);
			timers.pop();
		}
		
		now = time;
		
		// Any rescheduled timer will be executed in next SetNow() call
		for (auto& t : pendingTimers)
		{
			timers.push(t);
		}
		
		pendingTimers.clear();
	}

	virtual const std::chrono::milliseconds GetNow() const override
	{
		return now;
	}
	
	virtual Timer::shared CreateTimer(const std::function<void(std::chrono::milliseconds)>& callback) override
	{
		auto timer = std::make_shared<TimerImpl>(*this, callback);		
		return std::static_pointer_cast<Timer>(timer);
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
		func(now);
	}

	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback) override
	{
		func(now);
	}

	virtual std::future<void> Future(const std::function<void(std::chrono::milliseconds)>& func) override
	{
		func(now);
		return std::async(std::launch::deferred, []() {});
	}

private:
	using TimerSchedule = std::pair<std::chrono::milliseconds, TimerImpl::shared>;
	struct CompareFunc
	{
		bool operator()(const TimerSchedule& t1, const TimerSchedule& t2)
		{
			return t1.first > t2.first;
		}
	};
	
	std::chrono::milliseconds now;
	
	std::vector<TimerSchedule> pendingTimers;
	std::priority_queue<TimerSchedule, std::vector<TimerSchedule>, CompareFunc> timers;
};

#endif