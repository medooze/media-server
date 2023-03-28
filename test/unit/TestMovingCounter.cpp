#include "TestCommon.h"

#include "MovingCounter.h"


TEST(TestMovingMinCounter, Basic)
{
	MovingMinCounter<int64_t> minCounter(1000);
	minCounter.Add(1, -3000);

	for (int64_t i = 2; i < 5000; i++)
	{
		int64_t v = i % 2 ? -i : i;
		int64_t m = minCounter.Add(i, v);
		int64_t n = *minCounter.GetMin();

		//Log("time:%d val:%lld  min:%lld %lld\n", i, v, m, n);

		if (v < 0)
			ASSERT_EQ(m, n);
		if (i <= 1000)
			ASSERT_EQ(m, -3000);
		else if (v < 0)
			ASSERT_EQ(m, -i);
	}
}