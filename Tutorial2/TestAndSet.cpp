#include "stdafx.h"
#include "testAndSet.h"
using namespace std;

void TestAndSet::acquire() // pid is thread ID
{
	_asm{
			mov		ecx, this
		testAgain:
			mov		eax, 1							;
			xchg	eax, [ecx]TestAndSet.myLock		;
			test	eax, eax						;
			jne		testAgain						;
	}
}

void TestAndSet::release()
{
	myLock = 0;
}