#include "test.h"
#include "log.h"
#include "acumulator.h"
#include "MovingCounter.h"
#include "CircularBuffer.h"
#include <algorithm>

class ToolsPlan : public TestPlan
{
public:
	ToolsPlan() : TestPlan("Tools test plan")
	{

	}

	virtual void Execute()
	{
		testMovingCounter();
		testCircularQueue();
		testAccumulator();
		testCircularBuffer();

	}

	void testCircularQueue()
	{
		Log("-testCircularQueue\n");

		{
			Log("-testCircularQueue basic\n");

			CircularQueue<size_t> q;

			assert(q.empty());
			assert(q.full());
			assert(q.length()==0);

			q.push_back(1);
			assert(!q.empty());
			assert(q.length()==1);
			assert(q.front() == 1);

			q.pop_front();
			assert(q.empty());
			assert(q.length()==0);

			q.push_back(1);
			assert(!q.empty());
			assert(q.length() == 1);
			assert(q.front() == 1);

			q.pop_front();
			assert(q.empty());
			assert(q.length() == 0); 
		}

		{
			Log("-testCircularQueue push pop\n");

			CircularQueue<size_t> q;

			assert(q.empty());
			assert(q.full());
			assert(q.length() == 0);

			q.push_back(1);
			q.push_back(2);
			q.push_back(3);
			q.push_back(4);
			assert(!q.empty());
			assert(q.length() == 4);
			assert(q.front() == 1);

			q.pop_front();
			assert(q.length() == 3);
			assert(q.front() == 2);

			q.pop_front();
			assert(q.length() == 2);
			assert(q.front() == 3);

			q.pop_front();
			assert(q.length() == 1);
			assert(q.front() == 4);

			q.pop_front();
			assert(q.empty());
			assert(q.length() == 0);
		}

		{
			Log("-testCircularQueue autogrow\n");

			CircularQueue<size_t> q;

			for (size_t i = 0; i < 256; ++i)
			{
				q.push_back(i);
				assert(q.length() == i+1);
				for (size_t j=0; j<q.length(); j++)
					assert(q.at(j) == j);
			}

			for (size_t i = 0; i < 256; ++i)
			{
				assert(i == q.front());
				q.pop_front();
			}

			assert(q.empty());
			assert(q.length() == 0);
		}


		{
			Log("-testCircularQueue grow incrementally\n");

			CircularQueue<size_t> q(10);

			assert(q.empty());
			assert(q.length() == 0);
			assert(q.size()==10);

			q.push_back(1);
			q.push_back(2);
			q.push_back(3);

			assert(!q.empty());
			assert(q.length() == 3);
			assert(q.front() == 1);

			q.pop_front();
			q.pop_front();
			q.pop_front();
			assert(q.empty());
			assert(q.length() == 0);

			for (size_t i = 0; i < 10; ++i)
			{
				q.push_back(i + 10);
				assert(q.length() == i + 1);
				for (size_t j = 0; j < q.length(); j++)
				{
					//Log("%d %d %d\n",j, q.at(j), j+10);
					assert(q.at(j) == j + 10);
				}
			}

			//Grow but don't move everything to the new allocated space
			Log("grow 12\n");
			q.grow(12);
			assert(q.length() == 10);
			assert(q.size() == 12);
			for (size_t j = 0; j < q.length(); j++)
			{
				//Log("%d %d %d\n", j, q.at(j), j + 10);
				assert(q.at(j) == j + 10);
			}

			//Evrything should go to the new allocated space
			Log("grow 12\n");
			q.grow(14);
			assert(q.length() == 10);
			assert(q.size() == 14);
			for (size_t j = 0; j < q.length(); j++)
			{
				//Log("%d %d %d\n", j, q.at(j), j + 10);
				assert(q.at(j) == j + 10);
			}

			//Should be no change
			Log("grow 20\n");
			q.grow(20);
			assert(q.length() == 10);
			assert(q.size() == 20);
			for (size_t j = 0; j < q.length(); j++)
			{
				//Log("%d %d %d\n", j, q.at(j), j + 10);
				assert(q.at(j) == j + 10);
			}

			//Do not shrink
			q.grow(10);
			assert(q.size() == 20);


		}

		{
			Log("-testCircularQueue grow exact\n");

			CircularQueue<size_t> q(10);

			assert(q.empty());
			assert(q.length() == 0);
			assert(q.size() == 10);

			q.push_back(1);
			q.push_back(2);
			q.push_back(3);

			assert(!q.empty());
			assert(q.length() == 3);
			assert(q.front() == 1);

			q.pop_front();
			q.pop_front();
			q.pop_front();
			assert(q.empty());
			assert(q.length() == 0);

			for (size_t i = 0; i < 10; ++i)
			{
				q.push_back(i + 10);
				assert(q.length() == i + 1);
				for (size_t j = 0; j < q.length(); j++)
				{
					//Log("%d %d %d\n", j, q.at(j), j + 10);
					assert(q.at(j) == j + 10);
				}
			}

			//Grow exactly
			Log("grow 13\n");
			q.grow(13);
			assert(q.length() == 10);
			assert(q.size() == 13);
			for (size_t j = 0; j < q.length(); j++)
			{
			       // Log("%d %d %d\n", j, q.at(j), j + 10);
				assert(q.at(j) == j + 10);
			}
		}

		{
			Log("-testCircularQueue grow more\n");

			CircularQueue<size_t> q(10);

			assert(q.empty());
			assert(q.length() == 0);
			assert(q.size() == 10);

			q.push_back(1);
			q.push_back(2);
			q.push_back(3);

			assert(!q.empty());
			assert(q.length() == 3);
			assert(q.front() == 1);

			q.pop_front();
			q.pop_front();
			q.pop_front();
			assert(q.empty());
			assert(q.length() == 0);

			for (size_t i = 0; i < 10; ++i)
			{
				q.push_back(i + 10);
				assert(q.length() == i + 1);
				for (size_t j = 0; j < q.length(); j++)
				{
					//Log("%d %d %d\n", j, q.at(j), j + 10);
					assert(q.at(j) == j + 10);
				}
			}

			//Grow one more
			Log("grow 14\n");
			q.grow(14);
			assert(q.length() == 10);
			assert(q.size() == 14);
			for (size_t j = 0; j < q.length(); j++)
			{
				//Log("%d %d %d\n", j, q.at(j), j + 10);
				assert(q.at(j) == j + 10);
			}
		}
	}

	void testAccumulator()
	{

		Log("-testAccumulator\n");
		{
			Acumulator acu(10);
			MovingMinCounter<DWORD> min(10);
			MovingMaxCounter<DWORD> max(10);


			for (uint64_t i = 10; i < 1E6; i++)
			{
				DWORD val = rand();
				acu.Update(i, val);
				min.Add(i, val);
				max.Add(i, val);
				assert(acu.GetMaxValueInWindow() == *max.GetMax());
				assert(acu.GetMinValueInWindow() == *min.GetMin());


			}
		}

		{
			std::vector<std::pair<uint64_t,DWORD>> values;

			Acumulator acu(1000);
			MovingMinCounter<DWORD> min(1000);
			MovingMaxCounter<DWORD> max(1000);
			uint64_t ini = 0;
			for (uint64_t i = 0; i < 1E6; i++)
			{
				DWORD diff = 1000 * ((double)rand() / (RAND_MAX));
				DWORD val = 1000 * ((double)rand() / (RAND_MAX));
				ini += diff;
				acu.Update(ini, val);
				values.emplace_back(ini,val);
				values.erase(std::remove_if(values.begin(), values.end(), [=](auto& pair) { return pair.first + 1000 <= ini; }), values.end());
				auto instant = 0;
				for (const auto& value : values)
				{
					//Log("%d %d\n", value.first, value.second);
					instant += value.second;
				}
				min.Add(ini, val);
				max.Add(ini, val);
				//Log("time:%d val:%d min:%d max:%d instant:%d acu:[%d,%d,%d] \n", ini, val, *min.GetMin(), *max.GetMax(), instant, acu.GetMinValueInWindow(), acu.GetMaxValueInWindow(), acu.GetInstant());
				assert(acu.GetInstant() == instant);
				assert(acu.GetMaxValueInWindow() == *max.GetMax());
				assert(acu.GetMinValueInWindow() == *min.GetMin());
			}
		}
	}

	void testMovingCounter()
	{
			MovingMinCounter<int64_t> minCounter(1000);
			minCounter.Add(1, -3000);
	
			for (int64_t i = 2; i < 5000; i++)
			{
				int64_t v = i % 2 ? -i : i;
				int64_t m = minCounter.Add(i,v);
				int64_t n = *minCounter.GetMin();

				Log("time:%d val:%lld  min:%lld %lld\n", i, v, m, n);

				if (v<0)
					assert(m == n);
				if (i<=1000)
					assert(m == -3000);
				else if (v<0)
					assert(m == -i);
			}
	}

	void testCircularBuffer()
	{
		Log("-testCircularBuffer\n");

		{
			Log("-testCircularBuffer secuential\n");
			CircularBuffer<uint16_t, uint8_t, 10> buffer;

			assert(!buffer.IsPresent(0));
			for (uint16_t i = 0; i < 399; ++i)
			{
				assert(buffer.Set(i, i));
				//Log("%i %i %i %i %d\n", i, buffer.Get(i).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
				assert(buffer.IsPresent(i)); assert(buffer.Get(i).value() == i); assert(buffer.GetLastSeq() == i % 256);
			}

			assert(!buffer.Set(buffer.GetLastSeq() - 11, 0));
		}

		{
			Log("-testCircularBuffer evens\n");
			CircularBuffer<uint16_t, uint8_t, 10> buffer;

			assert(!buffer.IsPresent(0));
			for (uint16_t i = 0; i < 399; ++i)
			{
				assert(buffer.Set(i * 2, i));
				//Log("%i %i %i %i %d\n", i, buffer.Get(i * 2).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
				assert(buffer.IsPresent(i * 2)); assert(buffer.Get(i * 2).value() == i); assert(buffer.GetLastSeq() == (i * 2) % 256);
				if (i) assert(!buffer.IsPresent(i * 2 - 1));
			}

			assert(!buffer.Set(buffer.GetLastSeq() - 11, 0));
		}


		{
			Log("-testCircularBuffer big jumps\n");
			CircularBuffer<uint16_t, uint8_t, 10> buffer;

			assert(!buffer.IsPresent(0));
			for (uint16_t i = 0; i < 399; ++i)
			{
				assert(buffer.Set(i * 20, i));
				//Log("%i %i %i %i %d\n", i, buffer.Get(i * 20).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
				assert(buffer.IsPresent(i * 20)); assert(buffer.Get(i * 20).value() == i); assert(buffer.GetLastSeq() == (i * 20) % 256);
				if (i) assert(!buffer.IsPresent(i * 20 - 1));
			}

			assert(!buffer.Set(buffer.GetLastSeq() - 11, 0));
		}

		{
			Log("-testCircularBuffer wrap\n");

			CircularBuffer<uint32_t, uint32_t, 32768> buffer;

			assert(buffer.Set(-10, 1));
			Log("%i %i %d\n", buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());

			assert(buffer.IsPresent(-10));
			assert(buffer.Get(-10)==1);
			assert(buffer.GetFirstSeq() == -10);
			assert(buffer.GetLastSeq() == -10);


			assert(buffer.Set(1, 2));
			Log("%i %i %d\n", buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
			assert(buffer.IsPresent(1));
			assert(buffer.Get(1) == 2);
			assert(buffer.GetFirstSeq() == -10);
			assert(buffer.GetLastSeq() == 1);

		}
	}

};

ToolsPlan tools;

