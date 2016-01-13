#ifndef TESTANDTESTANDSET_H
#define TESTANDTESTANDSET_H	
class TestAndTestAndSet{
public:
	volatile long myLock;
	void acquire();
	void release();
};
#endif