#ifndef TICKET_H
#define TICKET_H	
class TicketLock {
public:
	volatile long ticket;
	volatile long nowServing;
	TicketLock();
	void acquire();
	void release();
};
#endif