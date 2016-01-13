/*
// sharing.cpp
//
// Copyright (C) 2013 - 2015 jones@scss.tcd.ie
// NB: hints for pasting from console window
// NB: Edit -> Select All followed by Edit -> Copy
// NB: paste into Excel using paste "Use Text Import Wizard" option and select "/" as the delimiter
*/

#include "stdafx.h"                             // pre-compiled headers
#include <iostream>                             // cout
#include <iomanip>                              // setprecision
#include "helper.h"                             //
#include "bakery.h"
#include "ticket.h"
#include "testAndSet.h"
#include "testAndTestAndSet.h"
#include "mcs.h"
/*********************************************
          My Variables
***********************************************/
BakeryLock bakery;
TicketLock ticket;
TestAndSet tAndS;
TestAndTestAndSet tAndTAndS;
MCS mcs;
QNode **lock = &mcs.lock;
DWORD tlsIndex  = TlsAlloc();
int nt = 1;

using namespace std;                            // cout

#define K           1024                        //
#define GB          (K*K*K)                     //
#define NOPS        10                       //
#define NSECONDS    6                           // run each test for NSECONDS

#define COUNTER64                               // comment for 32 bit counter
//#define FALSESHARING                          // allocate counters in same cache line
//#define USEPMS                                // use PMS counters

#ifdef COUNTER64
#define VINT    UINT64                          //  64 bit counter
#else
#define VINT    UINT                            //  32 bit counter
#endif

#define ALIGNED_MALLOC(sz, align) _aligned_malloc(sz, align)

#ifdef FALSESHARING
#define GINDX(n)    (g+n)                       //
#else
#define GINDX(n)    (g+n*lineSz/sizeof(VINT))   //
#endif

//
// OPTYP
//
// 
// 1:InterlockedIncrement
// 2:Bakery
// 3:Test and Set 
// 4:Test and Test and Set
// 5:MCS

#define OPTYP       5                          // set op type

#if OPTYP == 1

#ifdef COUNTER64
#define OPSTR       "InterlockedIncrement64"
#define INC(g)      InterlockedIncrement64((volatile LONG64*) g)
#else
#define OPSTR       "InterlockedIncrement"
#define INC(g)      InterlockedIncrement(g)
#endif

#elif OPTYP == 2

#define OPSTR		"BakeryLock"
#define INC(g)		{                                                                           \
						bakery.acquire(thread);                                                 \
						(*g)++;																    \
						bakery.release(thread);											        \
					} 

#elif OPTYP == 3
#define OPSTR		"TestAndSet"
#define INC(g)		{                                                                           \
						tAndS.acquire();														\
						(*g)++;																    \
						tAndS.release();														\
					}
#elif OPTYP == 4
#define OPSTR		"TestAndTestAndSet"
#define INC(g)		{                                                                           \
						tAndTAndS.acquire();													\
						(*g)++;																    \
						tAndTAndS.release();													\
					}

#elif OPTYP == 5
#define OPSTR		"MCS"
#define INC(g)		{                                                                           \
						mcs.acquire(lock, tlsIndex);													\
						(*g)++;																	\
						mcs.release(lock, tlsIndex);													\
					}
#endif

UINT64 tstart;                                  // start of test in ms
int sharing;                                    // % sharing
int lineSz;                                     // cache line size
int maxThread;                                  // max # of threads

THREADH *threadH;                               // thread handles
UINT64 *ops;                                    // for ops per thread

typedef struct {
    int sharing;                                // sharing
    int nt;                                     // # threads
    UINT64 rt;                                  // run time (ms)
    UINT64 ops;                                 // ops
    UINT64 incs;                                // should be equal ops
    UINT64 aborts;                              //
} Result;

Result *r;                                      // results
UINT indx;                                      // results index

volatile VINT *g;                               // NB: position of volatile

//
// test memory allocation [see lecture notes]
//
ALIGN(64) UINT64 cnt0;
ALIGN(64) UINT64 cnt1;
ALIGN(64) UINT64 cnt2;
UINT64 cnt3;                                    // NB: in Debug mode allocated in cache line occupied by cnt0

//
// PMS
//
#ifdef USEPMS

UINT64 *fixedCtr0;                              // fixed counter 0 counts
UINT64 *fixedCtr1;                              // fixed counter 1 counts
UINT64 *fixedCtr2;                              // fixed counter 2 counts
UINT64 *pmc0;                                   // performance counter 0 counts
UINT64 *pmc1;                                   // performance counter 1 counts
UINT64 *pmc2;                                   // performance counter 2 counts
UINT64 *pmc3;                                   // performance counter 2 counts

//
// zeroCounters
//
void zeroCounters()
{
    for (UINT i = 0; i < ncpu; i++) {
        for (int j = 0; j < 4; j++) {
            if (j < 3)
                writeFIXED_CTR(i, j, 0);
            writePMC(i, j, 0);
        }
    }
}

//
// void setupCounters()
//
void setupCounters()
{
    if (!openPMS())
        quit();

    //
    // enable FIXED counters
    //
    for (UINT i = 0; i < ncpu; i++) {
        writeFIXED_CTR_CTRL(i, (FIXED_CTR_RING123 << 8) | (FIXED_CTR_RING123 << 4) | FIXED_CTR_RING123);
        writePERF_GLOBAL_CTRL(i, (0x07ULL << 32) | 0x0f);
    }
}

//
// void saveCounters()
//
void saveCounters()
{
    for (UINT i = 0; i < ncpu; i++) {
        fixedCtr0[indx*ncpu + i] = readFIXED_CTR(i, 0);
        fixedCtr1[indx*ncpu + i] = readFIXED_CTR(i, 1);
        fixedCtr2[indx*ncpu + i] = readFIXED_CTR(i, 2);
        pmc0[indx*ncpu + i] = readPMC(i, 0);
        pmc1[indx*ncpu + i] = readPMC(i, 1);
        pmc2[indx*ncpu + i] = readPMC(i, 2);
        pmc3[indx*ncpu + i] = readPMC(i, 3);
    }
}

#endif

/*****************************************************
						WORKER
******************************************************/
WORKER worker(void *vthread)
{
#if OPTYP == 5
	QNode *qn = new QNode();
	TlsSetValue(tlsIndex, qn);
#endif

	int thread = (int)((size_t) vthread);

    UINT64 n = 0;

    volatile VINT *gt = GINDX(thread);
    volatile VINT *gs = GINDX(maxThread);

    runThreadOnCPU(thread % ncpu);

    while (1) {

        //
        // do some work
        //
        for (int i = 0; i < NOPS; i++) {
			INC(gs);
        }
        n += NOPS;

        //
        // check if runtime exceeded
        //
        if ((getWallClockMS() - tstart) > NSECONDS*1000)
            break;

    }
    ops[thread] = n;
    return 0;

}

/***************************************************************************
						 MAIN
***************************************************************************/
int main()
{
    ncpu = getNumberOfCPUs();   // number of logical CPUs
    maxThread = 2 * ncpu;       // max number of threads (originally 2*ncpu) on current windows machine cpus=4 logical cpus = 8 but getNumberOfCPUs() is returning the amount of logical processors 

    //
    // get date
    //
    char dateAndTime[256];
    getDateAndTime(dateAndTime, sizeof(dateAndTime));

    //
    // console output
    //
    cout << getHostName() << " " << getOSName() << " sharing " << (is64bitExe() ? "(64" : "(32") << "bit EXE)" ;
#ifdef _DEBUG
    cout << " DEBUG";
#else
    cout << " RELEASE";
#endif
    cout << " [" << OPSTR << "]" << " NCPUS=" << ncpu << " RAM=" << (getPhysicalMemSz() + GB - 1) / GB << "GB " << dateAndTime << endl;
#ifdef COUNTER64
    cout << "COUNTER64";
#else
    cout << "COUNTER32";
#endif
#ifdef FALSESHARING
    cout << " FALSESHARING";
#endif
    cout << " NOPS=" << NOPS << " NSECONDS=" << NSECONDS << " OPTYP=" << OPTYP;
#ifdef USEPMS
    cout << " USEPMS";
#endif
    cout << endl;
    cout << "Intel" << (cpu64bit() ? "64" : "32") << " family " << cpuFamily() << " model " << cpuModel() << " stepping " << cpuStepping() << " " << cpuBrandString() << endl;
#ifdef USEPMS
    cout << "performance monitoring version " << pmversion() << ", " << nfixedCtr() << " x " << fixedCtrW() << "bit fixed counters, " << npmc() << " x " << pmcW() << "bit performance counters" << endl;
#endif

    //
    // get cache info
    //
    lineSz = getCacheLineSz();
    //lineSz *= 2;

    if ((&cnt3 >= &cnt0) && (&cnt3 < (&cnt0 + lineSz/sizeof(UINT64))))
        cout << "Warning: cnt3 shares cache line used by cnt0" << endl;
    if ((&cnt3 >= &cnt1) && (&cnt3 < (&cnt1 + lineSz / sizeof(UINT64))))
        cout << "Warning: cnt3 shares cache line used by cnt1" << endl;
    if ((&cnt3 >= &cnt2) && (&cnt3 < (&cnt2 + lineSz / sizeof(UINT64))))
        cout << "Warning: cnt2 shares cache line used by cnt1" << endl;

    cout << endl;

    //
    // allocate global variable
    //
    // NB: each element in g is stored in a different cache line to stop false sharing
    //
    threadH = (THREADH*) ALIGNED_MALLOC(maxThread*sizeof(THREADH), lineSz);             // thread handles
    ops = (UINT64*) ALIGNED_MALLOC(maxThread*sizeof(UINT64), lineSz);                   // for ops per thread

#ifdef FALSESHARING
    g = (VINT*) ALIGNED_MALLOC((maxThread+1)*sizeof(VINT), lineSz);                     // local and shared global variables
#else
    g = (VINT*) ALIGNED_MALLOC((maxThread + 1)*lineSz, lineSz);                         // local and shared global variables
#endif

#ifdef USEPMS

    fixedCtr0 = (UINT64*) ALIGNED_MALLOC(5*maxThread*ncpu*sizeof(UINT64), lineSz);      // for fixed counter 0 results
    fixedCtr1 = (UINT64*) ALIGNED_MALLOC(5*maxThread*ncpu*sizeof(UINT64), lineSz);      // for fixed counter 1 results
    fixedCtr2 = (UINT64*) ALIGNED_MALLOC(5*maxThread*ncpu*sizeof(UINT64), lineSz);      // for fixed counter 2 results
    pmc0 = (UINT64*) ALIGNED_MALLOC(5*maxThread*ncpu*sizeof(UINT64), lineSz);           // for performance counter 0 results
    pmc1 = (UINT64*) ALIGNED_MALLOC(5*maxThread*ncpu*sizeof(UINT64), lineSz);           // for performance counter 1 results
    pmc2 = (UINT64*) ALIGNED_MALLOC(5*maxThread*ncpu*sizeof(UINT64), lineSz);           // for performance counter 2 results
    pmc3 = (UINT64*) ALIGNED_MALLOC(5*maxThread*ncpu*sizeof(UINT64), lineSz);           // for performance counter 3 results

#endif

    r = (Result*) ALIGNED_MALLOC(5*maxThread*sizeof(Result), lineSz);                   // for results
    memset(r, 0, 5*maxThread*sizeof(Result));                                           // zero

    indx = 0;

#ifdef USEPMS
    //
    // set up performance monitor counters
    //
    setupCounters();
#endif

    //
    // use thousands comma separator
    //
    setCommaLocale();

    //
    // header
    //
    cout << "sharing";
    cout << setw(4) << "nt";
    cout << setw(6) << "rt";
    cout << setw(16) << "ops";
    cout << setw(6) << "rel";
    cout << endl;

    cout << "-------";              // sharing
    cout << setw(4) << "--";        // nt
    cout << setw(6) << "--";        // rt
    cout << setw(16) << "---";      // ops
    cout << setw(6) << "---";       // rel
    cout << endl;

    //
    // boost process priority
    // boost current thread priority to make sure all threads created before they start to run
    //
#ifdef WIN32
//  SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
//  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif

    //
    // run tests
    //
    UINT64 ops1 = 1;

    //for (sharing = 0; sharing <= 100; sharing += 25) {

        for (nt = 1; nt <= maxThread; nt++, indx++) {

            //
            //  zero shared memory
            //
            for (int thread = 0; thread < nt; thread++)
                *(GINDX(thread)) = 0;   // thread local
            *(GINDX(maxThread)) = 0;    // shared

#ifdef USEPMS
            zeroCounters();             // zero PMS counters
#endif

			bakery.setThreads(nt);
			bakery.resetNumbers();
            //
            // get start time
            //
            tstart = getWallClockMS();

            //
            // create worker threads
            //
            for (int thread = 0; thread < nt; thread++)
                createThread(&threadH[thread], worker, (void*)(size_t)thread);

            //
            // wait for ALL worker threads to finish
            //
            waitForThreadsToFinish(nt, threadH);
            UINT64 rt = getWallClockMS() - tstart;

#ifdef USEPMS
            saveCounters();             // save PMS counters
#endif

            //
            // save results and output summary to console
            //
            for (int thread = 0; thread < nt; thread++) {
                r[indx].ops += ops[thread];
                r[indx].incs += *(GINDX(thread));
            }
            r[indx].incs += *(GINDX(maxThread));
            if ((sharing == 0) && (nt == 1))
                ops1 = r[indx].ops;
            r[indx].sharing = sharing;
            r[indx].nt = nt;
            r[indx].rt = rt;

            cout << setw(6) << sharing << "%";
            cout << setw(4) << nt;
            cout << setw(6) << fixed << setprecision(2) << (double) rt / 1000;
            cout << setw(16) << r[indx].ops;
            cout << setw(6) << fixed << setprecision(2) << (double) r[indx].ops / ops1;

            if (r[indx].ops != r[indx].incs)
                cout << " ERROR incs " << setw(3) << fixed << setprecision(0) << 100.0 * r[indx].incs / r[indx].ops << "% effective";

            cout << endl;

            //
            // delete thread handles
            //
            for (int thread = 0; thread < nt; thread++)
                closeThread(threadH[thread]);
        }

    //}

    cout << endl;

    //
    // output results so they can easily be pasted into a spread sheet from console window
    //
    setLocale();
    cout << "sharing/nt/rt/ops/incs";
    cout << endl;
    for (UINT i = 0; i < indx; i++) {
        cout << r[i].sharing << "/"  << r[i].nt << "/" << r[i].rt << "/"  << r[i].ops << "/" << r[i].incs;      
        cout << endl;
    }
    cout << endl;

#ifdef USEPMS

    //
    // output PMS counters
    //
    cout << "FIXED_CTR0 instructions retired" << endl;
    for (UINT i = 0; i < indx; i++) {
        for (UINT j = 0; j < ncpu; j++)
            cout << ((j) ? "/" : "") << fixedCtr0[i*ncpu + j];
        cout << endl;
    }
    cout << endl;
    cout << "FIXED_CTR1 unhalted core cycles" << endl;
    for (UINT i = 0; i < indx; i++) {
        for (UINT j = 0; j < ncpu; j++)
            cout << ((j) ? "/" : "") << fixedCtr1[i*ncpu + j];
        cout << endl;
    }
    cout << endl;
    cout << "FIXED_CTR2 unhalted reference cycles" << endl;
    for (UINT i = 0; i < indx; i++) {
        for (UINT j = 0; j < ncpu; j++ )
            cout << ((j) ? "/" : "") << fixedCtr2[i*ncpu + j];
        cout << endl;
    }
    cout << endl;
    cout << "PMC0 RTM RETIRED START" << endl;
    for (UINT i = 0; i < indx; i++) {
        for (UINT j = 0; j < ncpu; j++ )
            cout << ((j) ? "/" : "") << pmc0[i*ncpu + j];
        cout << endl;
    }
    cout << endl;
    cout << "PMC1 RTM RETIRED COMMIT" << endl;
    for (UINT i = 0; i < indx; i++) {
        for (UINT j = 0; j < ncpu; j++ )
            cout << ((j) ? "/" : "") << pmc1[i*ncpu + j];
        cout << endl;
    }
    cout << endl;
    cout << "PMC2 unhalted core cycles in committed transactions" << endl;
    for (UINT i = 0; i < indx; i++) {
        for (UINT j = 0; j < ncpu; j++ )
            cout << ((j) ? "/" : "") << pmc2[i*ncpu + j];
        cout << endl;
    }
    cout << endl;
    cout << "PMC3 unhalted core cycles in committed and aborted transactions" << endl;
    for (UINT i = 0; i < indx; i++) {
        for (UINT j = 0; j < ncpu; j++ )
            cout << ((j) ? "/" : "") << pmc3[i*ncpu + j];
        cout << endl;
    }

    closePMS();                 // close PMS counters

#endif

    quit();

    return 0;

}

// eof
