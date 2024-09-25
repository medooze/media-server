#ifndef SCOPEDCLEANUP_H
#define SCOPEDCLEANUP_H

#include <functional>

/**
 * This class execute a function when the object is destructed. It can be used
 * to execute clean up task when exiting the owner's scope.
 */
class ScopedCleanUp
{
public:
	ScopedCleanUp(const std::function<void()>& func) :
		func(func)
	{
	}
	
	~ScopedCleanUp()
	{
		func();
	}
	
private:
	std::function<void()> func;
};

#endif
