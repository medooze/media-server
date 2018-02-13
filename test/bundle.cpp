#include "test.h"
#include <vector>
#include <thread>
#include <atomic>
#include "RTPBundleTransport.h"
	
class BundleTestPlan: public TestPlan
{
public:
	BundleTestPlan() : TestPlan("Overlay test plan")
	{
		
	}
	
	int send() 
	{
		
		
		bool stop = false;
		const size_t size = 1000;
		const uint8_t data[size] = {0};
		std::atomic<uint> count(0);
		const size_t simultaneous = 4;
		std::vector<std::thread> threads(simultaneous);
		const size_t duration = 10; //In seconds
		
		Log(">RTPBundleTransport stress send [threads:%u,duration:%d]\n",simultaneous,duration);
		
		//We will create an bundle transport and send as fast as possible to detect deadlocks
		RTPBundleTransport bundle;
		bundle.Init();
		
		//Set dummy destination
		ICERemoteCandidate dest("192.168.64.1",1000,nullptr);

		//Create all threads
		for (auto& thread : threads)
		{
			//Create thread
			thread = std::thread([&](){
				while(!stop)
				{
					bundle.Send(&dest,data,size);
					count++;
				}
			});
		}
		
		//Wait
		sleep(duration);
		//Stop
		stop = true;
		
		//Join all threads
		for (auto& thread : threads)
			thread.join();
		
		//Print
		Log("<RTPBundleTransport stressed [sent:%u]\n",uint(count));
		
		//OK
		return true;
	}
	
	virtual void Execute()
	{
		send();
	}
	
};

BundleTestPlan bundle;
