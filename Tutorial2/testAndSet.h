#ifndef TESTANDSET_H
#define TESTANDSET_H	
class TestAndSet{
public:
	volatile long  myLock;
	void acquire();
	void release();
};
#endif