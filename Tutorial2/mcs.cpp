#include "stdafx.h"
#include "mcs.h"
template <class QNode>
void* ALIGNEDMA<QNode>::operator new(size_t sz)
{
	int lineSz = getCacheLineSz();
	sz = (sz+lineSz-1)/lineSz*lineSz; // make sz a multiple of lineSz
	return _aligned_malloc(sz, lineSz); // allocate on a lineSz boundary
}

template <class QNode>
void ALIGNEDMA<QNode>::operator delete(void *p)
{
	_aligned_free(p); // free object
}

MCS::MCS()
{
	lock = NULL;
}

void MCS::acquire(QNode **lock, DWORD tlsIndex)
{
	volatile QNode *qn = (QNode*) TlsGetValue(tlsIndex);
	qn->next = NULL;
	volatile QNode *pred = (QNode*) InterlockedExchangePointer((PVOID) lock, (PVOID) qn);
	if(pred == NULL)
		return;
	qn->waiting = 1;
	pred->next = qn;
	while(qn->waiting);
}

void MCS::release(QNode **lock, DWORD tlsIndex)
{
	volatile QNode *qn = (QNode*) TlsGetValue(tlsIndex);
	volatile QNode *succ;
	if(!(succ = qn->next)){
		if(InterlockedCompareExchangePointer((PVOID*)lock, NULL, (PVOID) qn) == qn)
			return;
		do{
			succ = qn->next;
		}while(!succ);
	}
	succ->waiting = 0;
}