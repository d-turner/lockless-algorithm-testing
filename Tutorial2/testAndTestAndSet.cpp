#include "stdafx.h"
#include "testAndTestAndSet.h"
using namespace std;

void TestAndTestAndSet::acquire() // pid is thread ID
{
	/*_asm{
		testAgain:
			mov		ecx, this							;
			mov		eax, 1								;
			xchg	eax, [ecx]TestAndTestAndSet.myLock	;
			test	eax, eax							;
			jne		testAgain							;
	}/*
	do{
		while(myLock == 1)
			_mm_pause;
	}while(InterlockedExchange64(&myLock,1));*/
	_asm{
			mov		ecx, this							;
		testAgain:								;
			cmp		[ecx]TestAndTestAndSet.myLock, 0	;
			jne     testAgain							;
			mov		eax, 1								;
			xchg	eax, [ecx]TestAndTestAndSet.myLock	;
			test	eax, eax							;
			jne		testAgain							;
	}
}	

void TestAndTestAndSet::release()
{
	myLock = 0;
}