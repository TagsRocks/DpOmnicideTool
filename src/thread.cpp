////////////////////////////////////////////////////////////////
//
// DpOmniTool / Threads management
// (c) Pavel [VorteX] Timofeyev
// See LICENSE text file for a license agreement
//
////////////////////////////////

#include "thread.h"
#include "dpomnitool.h"
#include "cmd.h"

// get a new work for thread
int	GetWorkForThread(ThreadData *thread)
{
	int	r;

	WaitForSingleObject(thread->pool->work_mutex, INFINITE);
	if (thread->pool->work_pending >= thread->pool->work_num)
		r = -1;
	else
	{
		r = thread->pool->work_pending;
		thread->pool->work_pending++;
	}
	ReleaseMutex(thread->pool->work_mutex);
	return r;
}

/*
===================================================================

WIN32

===================================================================
*/

#ifdef WIN32

#include <windows.h>

int	num_cpu_cores = -1;

void Thread_Init(void)
{
	SYSTEM_INFO info;

	GetSystemInfo(&info);
	num_cpu_cores = info.dwNumberOfProcessors;
	if (num_cpu_cores < 1 || num_cpu_cores > 32)
		num_cpu_cores = 1;
}

void Thread_Shutdown(void)
{
}

// run thread in parallel
double ParallelThreads(int num_threads, int work_count, void *common_data, void(*thread_func)(ThreadData *thread), void(*central_thread)(ThreadData *thread))
{
	double start;
	ThreadPool pool = { 0 };
	ThreadData *threads;
	int	i;

	if (work_count <= 0 || num_threads <= 0 || !thread_func)
		return 0;

	start = TimeDouble();

	// init threading system
	if (num_cpu_cores == -1)
		Thread_Init();

	// create thread pool
	pool.work_num = work_count;
	pool.work_pending = 0;
	pool.work_mutex = CreateMutex(NULL, FALSE, NULL);
	pool.threads_num = min(num_threads, work_count) + ((central_thread) ? 1 : 0);
	pool.threads = mem_alloc(sizeof(ThreadData) * pool.threads_num);
	memset(pool.threads, 0, sizeof(ThreadData) * pool.threads_num);
	pool.finished = false;

	// start threads
	threads = (ThreadData *)pool.threads;
	for (i = 0; i < pool.threads_num; i++)
	{
		threads[i].num = i;
		threads[i].pool = &pool;
		threads[i].data = common_data;
	}

	// run works
	if (central_thread)
	{
		// run central thread and wait until it will initialize things
		threads[0].handle = CreateThread(NULL, THREAD_STACK_SIZE, (LPTHREAD_START_ROUTINE)central_thread, (LPVOID)&threads[0], 0, (LPDWORD)&threads[0].id);
		while(pool.started == false && pool.stop == false)
			Sleep(10);
		if (pool.stop == false)
		{
			// run works in paralel
			for (i = 1; i < pool.threads_num; i++)
				threads[i].handle = CreateThread(NULL, THREAD_STACK_SIZE, (LPTHREAD_START_ROUTINE)thread_func, (LPVOID)&threads[i], 0, (LPDWORD)&threads[i].id);
			for (i = 1; i < pool.threads_num; i++)
				WaitForSingleObject(threads[i].handle, INFINITE);
		}
		// set finished mark so central thread will know that we are finished
		pool.finished = true;
		WaitForSingleObject(threads[0].handle, INFINITE);
	}
	else
	{
		// run works in paralel
		for (i = 0; i < pool.threads_num; i++)
			threads[i].handle = CreateThread(NULL, THREAD_STACK_SIZE, (LPTHREAD_START_ROUTINE)thread_func, (LPVOID)&threads[i], 0, (LPDWORD)&threads[i].id);
		for (i = 0; i < pool.threads_num; i++)
			WaitForSingleObject(threads[i].handle, INFINITE);
	}

	// delete threads pool
	CloseHandle(pool.work_mutex);
	mem_free(pool.threads);

	// return whole time
	return TimeDouble() - start;
}

#else

#error "Threads not implemented!"

#endif