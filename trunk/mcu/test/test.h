/* 
 * File:   test.h
 * Author: Sergio
 *
 * Created on 7 de agosto de 2015, 10:31
 */

#ifndef TEST_H
#define	TEST_H
#include "log.h"
#include <set>
#include <string>
#include <ostream>  
#include <iostream>

class TestPlan
{
public:
	TestPlan(const char *name) : name(name)
	{
		Log("-Registering: %s\r\n", GetName().c_str());
		tests.insert(this);
	}
	virtual void Execute() = 0;
	
	std::string GetName() { return name; }
public:
	static int ExecuteAll() 
	{
		Log(">Executing all test plan\r\n");
		for (TestPlans::iterator it = tests.begin(); it != tests.end(); ++it)
		{
			TestPlan* test = *it;
			Log(">Executing: %s\r\n", test->GetName().c_str());
			test->Execute();
			Log("<Executed %s\r\n", test->GetName().c_str());
		}
		Log("<All plans executed \r\n");
		return 0;
	}
	
private:
	typedef std::set<TestPlan*> TestPlans;
	static TestPlans tests;
	std::string name;
};
	


#endif	/* TEST_H */

