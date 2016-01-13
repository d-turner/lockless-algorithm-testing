#ifndef MCS_H
#define MCS_H	
template <class T>
class ALIGNEDMA
{
public:
	void* operator new(size_t);
	void operator delete(void*);
};

class QNode
{
public:
	volatile int waiting;
	volatile QNode *next;
};

class MCS{
public:
	QNode *lock;
	MCS();
	void acquire(QNode**, DWORD);
	void release(QNode**, DWORD);
};
#endif