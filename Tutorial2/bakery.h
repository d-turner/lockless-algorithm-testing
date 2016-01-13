#ifndef BAKERY_H
#define BAKERY_H	
using namespace std;
class BakeryLock
{
public:
	static const int maxThreads= 16;
	unsigned long long number[maxThreads]; // thread IDs 0 to MAXTHREAD-1
	int choosing[maxThreads];
	int threads;
	void acquire(int);// pid is thread ID
	void release(int);
	void setThreads(int);
	void resetNumbers();
};
 
#endif