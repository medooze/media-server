#include "TestCommon.h"

#include "config.h"
#include "log.h"
#include "acumulator.h"
#include <algorithm>

TEST(TestAccumulator, MinMax)
{
	MinMaxAcumulator acu(10);
	MovingMinCounter<DWORD> min(10);
	MovingMaxCounter<DWORD> max(10);

	for (uint64_t i = 10; i < 1E6; i++)
	{
		DWORD val = rand();
		acu.Update(i, val);
		min.Add(i, val);
		max.Add(i, val);

		ASSERT_EQ(acu.GetMaxValueInWindow(), *max.GetMax());
		ASSERT_EQ(acu.GetMinValueInWindow(), *min.GetMin());


	}
}

TEST(TestAccumulator, Update)
{
	std::vector<std::pair<uint64_t, DWORD>> values;

	MinMaxAcumulator acu(1000);
	MovingMinCounter<DWORD> min(1000);
	MovingMaxCounter<DWORD> max(1000);
	uint64_t ini = 0;
	for (uint64_t i = 0; i < 1E6; i++)
	{
		DWORD diff = 1000 * ((double)rand() / (RAND_MAX));
		DWORD val = 1000 * ((double)rand() / (RAND_MAX));
		ini += diff;
		acu.Update(ini, val);
		values.emplace_back(ini, val);
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

		ASSERT_EQ(acu.GetInstant(), instant);
		ASSERT_EQ(acu.GetMaxValueInWindow(), *max.GetMax());
		ASSERT_EQ(acu.GetMinValueInWindow(), *min.GetMin());
	}
}

TEST(TestAccumulator, UpdateSameTimestamp)
{
	std::vector<std::pair<uint64_t, DWORD>> values;

	MinMaxAcumulator acu(1000);
	MovingMinCounter<DWORD> min(1000);
	MovingMaxCounter<DWORD> max(1000);
	uint64_t ini = 0;
	for (uint64_t i = 0; i < 1E6; i++)
	{
		DWORD diff = 1000 * ((double)rand() / (RAND_MAX));
		ini += diff;

		//Insert the multiple values with the same timestamp
		for (uint64_t j = 0; j < diff % 3; j++)
		{
			DWORD val = 1000 * ((double)rand() / (RAND_MAX));

			acu.Update(ini, val);
			values.emplace_back(ini, val);
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

			ASSERT_EQ(acu.GetInstant(), instant);
			ASSERT_EQ(acu.GetMaxValueInWindow(), *max.GetMax());
			ASSERT_EQ(acu.GetMinValueInWindow(), *min.GetMin());
		}
	}
}