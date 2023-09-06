/* 
 * File:   CPUMonitor.cpp
 * Author: Sergio
 * 
 * Created on 25 de marzo de 2014, 13:02
 */
#include <sys/time.h>
#include <sys/resource.h>

#include "CPUMonitor.h"

CPUMonitor::CPUMonitor()
{
	//Get num of cpus
	numcpu = sysconf( _SC_NPROCESSORS_ONLN );
	//Not started
	started = false;
}

void CPUMonitor::AddListener(Listener *listener)
{
	//Add it
	listeners.insert(listener);
}

void CPUMonitor::Start(int interval)
{
	//Check if started
	if (started)
		//Exit
		return;
	//Store interval
	this->interval = interval;

	//I am starte
	started = 1;

	//Create threads
	createPriorityThread(&thread,run,this,0);
}

void CPUMonitor::Stop()
{
	//Check we have been inited
	if (!started)
		//Do nothing
		return;

	//Stop thread
	started = 0;

	//Cancel wait
	wait.Cancel();

	//Wait for server thread to close
        pthread_join(thread,NULL);
}

void * CPUMonitor::run(void *par)
{
        Log("-CPU Monitor Thread [%lu]\n",pthread_self());

        //Obtenemos el parametro
        CPUMonitor *monitor = (CPUMonitor *)par;

        //Bloqueamos las señales
        blocksignals();

        //Ejecutamos
        monitor->Run();
	//Exit
	return NULL;
}

int CPUMonitor::Run()
{
	rusage prev;
	rusage next;
	timeval before = {};
	//Get previous measure
	getrusage(RUSAGE_SELF,&prev);
	//Get current time
	getUpdDifTime(&before);
	//While not stopped
	while (started && !wait.WaitSignal(interval))
	{
		//Get current usage
		getrusage(RUSAGE_SELF,&next);
		//Get time diference
		QWORD diff = getUpdDifTime(&before);
		//Calculate usage
		QWORD uTime	= (((QWORD)next.ru_utime.tv_sec)*1000000+next.ru_utime.tv_usec) - (((QWORD)prev.ru_utime.tv_sec)*1000000+prev.ru_utime.tv_usec);
		QWORD sTime	= (((QWORD)next.ru_stime.tv_sec)*1000000+next.ru_stime.tv_usec) - (((QWORD)prev.ru_stime.tv_sec)*1000000+prev.ru_stime.tv_usec);
		//Copy usage data
		memcpy((BYTE*)&prev,(BYTE*)&next,sizeof(rusage));
		//Get load
		int user  =  uTime*100/(diff*numcpu);
		int sys   =  sTime*100/(diff*numcpu);
		int load  = (uTime+sTime)*100/(diff*numcpu);
		//Send listener
		Debug("-CPU monitor usage %d %d %d %d\n",user,sys,load,numcpu);
		//Call listeners
		for(Listeners::iterator it = listeners.begin(); it!=listeners.end(); ++it)
			//call it
			(*it)->onCPULoad(user,sys,load,numcpu);
	}

	return 0;
}