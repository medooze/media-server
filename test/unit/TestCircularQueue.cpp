#include "TestCommon.h"
#include "CircularQueue.h"

TEST(TestCircularQueue, Basic)
{
	CircularQueue<size_t> q;

	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.full());
	ASSERT_EQ(q.length(), 0);

	q.push_back(1);
	ASSERT_TRUE(!q.empty());
	ASSERT_EQ(q.length(), 1);
	ASSERT_EQ(q.front(), 1);

	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);

	q.push_back(1);
	ASSERT_TRUE(!q.empty());
	ASSERT_EQ(q.length(), 1);
	ASSERT_EQ(q.front(), 1);

	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);
}

TEST(TestCircularQueue, PushPop)
{
	CircularQueue<size_t> q;

	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.full());
	ASSERT_EQ(q.length(), 0);

	q.push_back(1);
	ASSERT_EQ(q.at(0), 1);
	q.push_back(2);
	ASSERT_EQ(q.at(1), 2);
	q.push_back(3);
	ASSERT_EQ(q.at(2), 3);
	q.push_back(4);
	ASSERT_EQ(q.at(3), 4);
	ASSERT_TRUE(!q.empty());
	ASSERT_EQ(q.length(), 4);
	ASSERT_EQ(q.front(), 1);

	q.pop_front();
	ASSERT_EQ(q.length(), 3);
	ASSERT_EQ(q.front(), 2);

	q.pop_front();
	ASSERT_EQ(q.length(), 2);
	ASSERT_EQ(q.front(), 3);

	q.pop_front();
	ASSERT_EQ(q.length(), 1);
	ASSERT_EQ(q.front(), 4);

	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);
}

TEST(TestCircularQueue, AutoGrow)
{
	CircularQueue<size_t> q;

	for (size_t i = 0; i < 256; ++i)
	{
		q.push_back(i);
		ASSERT_EQ(q.length(), i + 1);
		for (size_t j = 0; j < q.length(); j++)
			ASSERT_EQ(q.at(j), j);
	}

	for (size_t i = 0; i < 256; ++i)
	{
		ASSERT_EQ(i, q.front());
		q.pop_front();
	}

	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);
}

TEST(TestCircularQueue, GrowIncrementally)
{
	CircularQueue<size_t> q(10);

	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);
	ASSERT_EQ(q.size(), 10);

	q.push_back(1);
	q.push_back(2);
	q.push_back(3);

	ASSERT_TRUE(!q.empty());
	ASSERT_EQ(q.length(), 3);
	ASSERT_EQ(q.front(), 1);

	q.pop_front();
	q.pop_front();
	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);

	for (size_t i = 0; i < 10; ++i)
	{
		q.push_back(i + 10);
		ASSERT_EQ(q.length(), i + 1);
		for (size_t j = 0; j < q.length(); j++)
		{
			//Log("%d %d %d\n",j, q.at(j), j+10);
			ASSERT_EQ(q.at(j), j + 10);
		}
	}

	//Grow but don't move everything to the new allocated space
	//Log("grow 12\n");
	q.grow(12);
	ASSERT_EQ(q.length(), 10);
	ASSERT_EQ(q.size(), 12);
	for (size_t j = 0; j < q.length(); j++)
	{
		//Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_EQ(q.at(j), j + 10);
	}

	//Evrything should go to the new allocated space
	//Log("grow 12\n");
	q.grow(14);
	ASSERT_EQ(q.length(), 10);
	ASSERT_EQ(q.size(), 14);
	for (size_t j = 0; j < q.length(); j++)
	{
		//Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_EQ(q.at(j), j + 10);
	}

	//Should be no change
	//Log("grow 20\n");
	q.grow(20);
	ASSERT_EQ(q.length(), 10);
	ASSERT_EQ(q.size(), 20);
	for (size_t j = 0; j < q.length(); j++)
	{
		//Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_EQ(q.at(j), j + 10);
	}

	//Do not shrink
	q.grow(10);
	ASSERT_EQ(q.size(), 20);


}

TEST(TestCircularQueue, GrowExact)
{

	CircularQueue<size_t> q(10);

	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);
	ASSERT_EQ(q.size(), 10);

	q.push_back(1);
	q.push_back(2);
	q.push_back(3);

	ASSERT_TRUE(!q.empty());
	ASSERT_EQ(q.length(), 3);
	ASSERT_EQ(q.front(), 1);

	q.pop_front();
	q.pop_front();
	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);

	for (size_t i = 0; i < 10; ++i)
	{
		q.push_back(i + 10);
		ASSERT_EQ(q.length(), i + 1);
		for (size_t j = 0; j < q.length(); j++)
		{
			//Log("%d %d %d\n", j, q.at(j), j + 10);
			ASSERT_EQ(q.at(j), j + 10);
		}
	}

	//Grow exactly
	//Log("grow 13\n");
	q.grow(13);
	ASSERT_EQ(q.length(), 10);
	ASSERT_EQ(q.size(), 13);
	for (size_t j = 0; j < q.length(); j++)
	{
		// Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_EQ(q.at(j), j + 10);
	}
}

TEST(TestCircularQueue, GrowMore)
{
	CircularQueue<size_t> q(10);

	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);
	ASSERT_EQ(q.size(), 10);

	q.push_back(1);
	q.push_back(2);
	q.push_back(3);

	ASSERT_TRUE(!q.empty());
	ASSERT_EQ(q.length(), 3);
	ASSERT_EQ(q.front(), 1);

	q.pop_front();
	q.pop_front();
	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_EQ(q.length(), 0);

	for (size_t i = 0; i < 10; ++i)
	{
		q.push_back(i + 10);
		ASSERT_EQ(q.length(), i + 1);
		for (size_t j = 0; j < q.length(); j++)
		{
			//Log("%d %d %d\n", j, q.at(j), j + 10);
			ASSERT_EQ(q.at(j), j + 10);
		}
	}

	//Grow one more
	//Log("grow 14\n");
	q.grow(14);
	ASSERT_EQ(q.length(), 10);
	ASSERT_EQ(q.size(), 14);
	for (size_t j = 0; j < q.length(); j++)
	{
		//Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_EQ(q.at(j), j + 10);
	}
}

TEST(TestCircularQueue, NoGrowOverflow)
{
	// Testing the overflow behaviour used in the VideoPipe
	static const int QMAX = 1;
	CircularQueue<int> q(QMAX, false);

	// Lets make sure it starts in a different spot (internally in the array) each time but still has same external behaviour
	for (size_t iteration = 0; iteration < QMAX+1; ++iteration)
	{
		// Since the impl resets head each time it is empty, lets push some items on to start at a different location each iteration before running the test
		for (size_t i = 0; i < iteration; ++i)
		{
			q.push_back(i);
		}

		for (size_t i = 0; i < QMAX; ++i)
		{
			q.push_back(i+1);
			ASSERT_EQ(q.length(), std::min<size_t>(i + 1U + iteration, QMAX));
		}
		ASSERT_TRUE(q.full());
		ASSERT_FALSE(q.empty());
		ASSERT_EQ(q.length(), QMAX);

		// Will push and overwrite
		q.push_back(QMAX+1);
		ASSERT_TRUE(q.full());
		ASSERT_FALSE(q.empty());
		ASSERT_EQ(q.length(), QMAX);

		for (size_t i = 0; i < QMAX; ++i)
		{
			int item = 0;
			item = q.front();
			ASSERT_TRUE(q.pop_front());
			ASSERT_EQ(q.length(), QMAX - i - 1);
			ASSERT_EQ(item, i + 1 + 1);
		}

		ASSERT_TRUE(q.empty());
		ASSERT_FALSE(q.full());
		ASSERT_EQ(q.length(), 0);
		ASSERT_FALSE(q.pop_front());

	}
}
