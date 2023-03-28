#include "TestCommon.h"
#include "CircularQueue.h"

TEST(TestCircularQueue, Basic)
{
	CircularQueue<size_t> q;

	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.full());
	ASSERT_TRUE(q.length() == 0);

	q.push_back(1);
	ASSERT_TRUE(!q.empty());
	ASSERT_TRUE(q.length() == 1);
	ASSERT_TRUE(q.front() == 1);

	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);

	q.push_back(1);
	ASSERT_TRUE(!q.empty());
	ASSERT_TRUE(q.length() == 1);
	ASSERT_TRUE(q.front() == 1);

	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);
}

TEST(TestCircularQueue, PushPop)
{
	CircularQueue<size_t> q;

	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.full());
	ASSERT_TRUE(q.length() == 0);

	q.push_back(1);
	q.push_back(2);
	q.push_back(3);
	q.push_back(4);
	ASSERT_TRUE(!q.empty());
	ASSERT_TRUE(q.length() == 4);
	ASSERT_TRUE(q.front() == 1);

	q.pop_front();
	ASSERT_TRUE(q.length() == 3);
	ASSERT_TRUE(q.front() == 2);

	q.pop_front();
	ASSERT_TRUE(q.length() == 2);
	ASSERT_TRUE(q.front() == 3);

	q.pop_front();
	ASSERT_TRUE(q.length() == 1);
	ASSERT_TRUE(q.front() == 4);

	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);
}

TEST(TestCircularQueue, AutoGrow)
{
	CircularQueue<size_t> q;

	for (size_t i = 0; i < 256; ++i)
	{
		q.push_back(i);
		ASSERT_TRUE(q.length() == i + 1);
		for (size_t j = 0; j < q.length(); j++)
			ASSERT_TRUE(q.at(j) == j);
	}

	for (size_t i = 0; i < 256; ++i)
	{
		ASSERT_TRUE(i == q.front());
		q.pop_front();
	}

	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);
}

TEST(TestCircularQueue, GrowIncrementally)
{
	CircularQueue<size_t> q(10);

	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);
	ASSERT_TRUE(q.size() == 10);

	q.push_back(1);
	q.push_back(2);
	q.push_back(3);

	ASSERT_TRUE(!q.empty());
	ASSERT_TRUE(q.length() == 3);
	ASSERT_TRUE(q.front() == 1);

	q.pop_front();
	q.pop_front();
	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);

	for (size_t i = 0; i < 10; ++i)
	{
		q.push_back(i + 10);
		ASSERT_TRUE(q.length() == i + 1);
		for (size_t j = 0; j < q.length(); j++)
		{
			//Log("%d %d %d\n",j, q.at(j), j+10);
			ASSERT_TRUE(q.at(j) == j + 10);
		}
	}

	//Grow but don't move everything to the new allocated space
	//Log("grow 12\n");
	q.grow(12);
	ASSERT_TRUE(q.length() == 10);
	ASSERT_TRUE(q.size() == 12);
	for (size_t j = 0; j < q.length(); j++)
	{
		//Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_TRUE(q.at(j) == j + 10);
	}

	//Evrything should go to the new allocated space
	//Log("grow 12\n");
	q.grow(14);
	ASSERT_TRUE(q.length() == 10);
	ASSERT_TRUE(q.size() == 14);
	for (size_t j = 0; j < q.length(); j++)
	{
		//Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_TRUE(q.at(j) == j + 10);
	}

	//Should be no change
	//Log("grow 20\n");
	q.grow(20);
	ASSERT_TRUE(q.length() == 10);
	ASSERT_TRUE(q.size() == 20);
	for (size_t j = 0; j < q.length(); j++)
	{
		//Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_TRUE(q.at(j) == j + 10);
	}

	//Do not shrink
	q.grow(10);
	ASSERT_TRUE(q.size() == 20);


}

TEST(TestCircularQueue, GrowExact)
{

	CircularQueue<size_t> q(10);

	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);
	ASSERT_TRUE(q.size() == 10);

	q.push_back(1);
	q.push_back(2);
	q.push_back(3);

	ASSERT_TRUE(!q.empty());
	ASSERT_TRUE(q.length() == 3);
	ASSERT_TRUE(q.front() == 1);

	q.pop_front();
	q.pop_front();
	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);

	for (size_t i = 0; i < 10; ++i)
	{
		q.push_back(i + 10);
		ASSERT_TRUE(q.length() == i + 1);
		for (size_t j = 0; j < q.length(); j++)
		{
			//Log("%d %d %d\n", j, q.at(j), j + 10);
			ASSERT_TRUE(q.at(j) == j + 10);
		}
	}

	//Grow exactly
	//Log("grow 13\n");
	q.grow(13);
	ASSERT_TRUE(q.length() == 10);
	ASSERT_TRUE(q.size() == 13);
	for (size_t j = 0; j < q.length(); j++)
	{
		// Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_TRUE(q.at(j) == j + 10);
	}
}

TEST(TestCircularQueue, GrowMore)
{
	CircularQueue<size_t> q(10);

	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);
	ASSERT_TRUE(q.size() == 10);

	q.push_back(1);
	q.push_back(2);
	q.push_back(3);

	ASSERT_TRUE(!q.empty());
	ASSERT_TRUE(q.length() == 3);
	ASSERT_TRUE(q.front() == 1);

	q.pop_front();
	q.pop_front();
	q.pop_front();
	ASSERT_TRUE(q.empty());
	ASSERT_TRUE(q.length() == 0);

	for (size_t i = 0; i < 10; ++i)
	{
		q.push_back(i + 10);
		ASSERT_TRUE(q.length() == i + 1);
		for (size_t j = 0; j < q.length(); j++)
		{
			//Log("%d %d %d\n", j, q.at(j), j + 10);
			ASSERT_TRUE(q.at(j) == j + 10);
		}
	}

	//Grow one more
	//Log("grow 14\n");
	q.grow(14);
	ASSERT_TRUE(q.length() == 10);
	ASSERT_TRUE(q.size() == 14);
	for (size_t j = 0; j < q.length(); j++)
	{
		//Log("%d %d %d\n", j, q.at(j), j + 10);
		ASSERT_TRUE(q.at(j) == j + 10);
	}
}