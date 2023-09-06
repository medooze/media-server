/* 
 * File:   CPUMonitor.h
 * Author: Sergio
 *
 * Created on 25 de marzo de 2014, 13:02
 */

#ifndef CPUMONITOR_H
#define	CPUMONITOR_H

#include <set>
#include "wait.h"


class CPUMonitor
{
public:
	class Listener
	{
	public:
		virtual void onCPULoad(int user, int sys, int load, int numcpu) = 0;
	};
public:
	CPUMonitor();
	void Start(int interval);
	void AddListener(Listener *listener);
	void Stop();

protected:
        int Run();

private:
	typedef std::set<Listener*> Listeners;
        static void * run(void *par);

private:
	int		numcpu = 0;
	int		interval = 0;
	bool		started = false;
	pthread_t	thread = 0;
	Wait		wait;
	Listeners	listeners;
};

#endif	/* CPUMONITOR_H */

