#include "stdafx.h"
#include <stdlib.h>
#include "ticket.h"
TicketLock::TicketLock()
{
	ticket = 0;
	nowServing = 0;
}

void TicketLock::acquire()
{
	long myTicket = InterlockedExchangeAdd(&ticket, 1);
	while (myTicket != nowServing){
		Sleep(100);
	}
}
	
void TicketLock::release()
{
	nowServing++;
}
