#include "test.h"
#include "EventLoop.h"

class EventLoopTestPlan : public TestPlan
{
public:
	EventLoopTestPlan() : TestPlan("EventLoop")
	{
	}

	int init()
	{
		Log("EventLoop::Init\n");
		return true;
	}

	int end()
	{
		Log("EventLoop::End\n");
		return true;
	}


	virtual void Execute()
	{
		init();

		Log("testTasks\n");
		testTasks();


		end();
	}

	virtual void testTasks()
	{

		EventLoop main;
		EventLoop tester;
		
		main.Start();
		tester.Start();

		main.SetThreadName("main");
		tester.SetThreadName("tester");

		main.CreateTimer(0ms, 50ms, [&](std::chrono::milliseconds triggered) {});

		Timer::shared timer;
		timer = tester.CreateTimer(1ms,[&](std::chrono::milliseconds triggered){

			//Run in main thread
			main.Sync([&](std::chrono::milliseconds executed) {
				auto diff = executed - triggered;
				if (diff.count() > 10) Log("%u", diff.count());
				assert(diff.count()<=10);
				printf(".");
			});

			timer->Again(0ms);
		});

		sleep(3);

		tester.Stop();
		main.Stop();
	}

};

EventLoopTestPlan el;
