/*
 * 
 */
#include "test.h"

int main(int argc, char** argv)
{
	Logger::EnableDebug(true);
	return TestPlan::ExecuteAll();
}

