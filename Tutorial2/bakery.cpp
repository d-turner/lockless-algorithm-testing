#include "stdafx.h"
#include "bakery.h"
using namespace std;

void BakeryLock::acquire(int pid) // pid is thread ID
{
	choosing[pid] = 1;
	unsigned long long max = 0;
	for (int i = 0; i <threads; i++) { // find maximum ticket
		if (number[i] > max)
			max = number[i];
	}
	number[pid] = max + 1; // our ticket number is maximum ticket found + 1
	choosing[pid] = 0;
	_mm_mfence();
	for (int j = 0; j < threads; j++) { // wait until our turn i.e. have lowest ticket
		while (choosing[j]);
		while ((number[j] != 0) && ((number[j] < number[pid]) || ((number[j] == number[pid]) && (j < pid))));
	}
}

void BakeryLock::release(int pid)
{
	number[pid] = 0; // release lock
	_mm_mfence();
}

void BakeryLock::setThreads(int newThreads)
{
	threads = newThreads;
}

void BakeryLock::resetNumbers()
{
	for(int i=0; i<maxThreads; i++){
		number[i] = 0;
	}
}
/*
BakeryLock lock;
#define LOCKSTR "ticket lock"
#define INIT() lock.ticket = lock.nowServing = 0
#define ACQUIRE() lock.acquire()
#define RELEASE() lock.release();
*/