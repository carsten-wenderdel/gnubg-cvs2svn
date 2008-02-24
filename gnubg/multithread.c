/* This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#if USE_MULTITHREAD
#ifdef WIN32
#include <windows.h>
#include <process.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gthread.h>
#if USE_GTK
#include <gtk/gtk.h>
#endif

#include "multithread.h"
#include "speed.h"
#include "rollout.h"
#include "util.h"

#define UI_UPDATETIME 250

#if __GNUC__
#define GCC_ALIGN_HACK 1
#endif

#if TRY_COUNTING_PROCEESING_UNITS
extern int GetLogicalProcssingUnitCount(void);
#endif

#ifdef GLIB_THREADS
typedef struct _ManualEvent
{
    GCond* cond;
    int signalled;
} *ManualEvent;
typedef GPrivate* TLSItem;
typedef GCond* Event;
typedef GMutex* Mutex;
GMutex* condMutex=NULL;    /* Extra mutex needed for waiting */
GAsyncQueue *async_queue=NULL; /* Needed for async waiting */
#else
typedef HANDLE ManualEvent;
typedef DWORD TLSItem;
typedef HANDLE Event;
typedef HANDLE Mutex;
#endif

typedef struct _ThreadData
{
    ManualEvent activity;
    TLSItem tlsItem;
    Event alldone;
    Mutex queueLock;
    Mutex multiLock;
	ManualEvent syncStart;
	ManualEvent syncEnd;

    int addedTasks;
    int doneTasks;
    int totalTasks;

    GList *tasks;
    int result;
} ThreadData;

ThreadData td;

static double start; /* used for timekeeping */
static unsigned int numThreads = 0;

#ifdef GLIB_THREADS

static void TLSCreate(TLSItem *pItem)
{
    *pItem = g_private_new(free);
}

static void TLSFree(TLSItem pItem)
{	/* Done automaticaly by glib */
}

static void TLSSetValue(TLSItem pItem, int value)
{
	int *pNew = (int*)malloc(sizeof(int));
	*pNew = value;
    g_private_set(pItem, (gpointer)pNew);
}

#define TLSGet(item) *((int*)g_private_get(item))

static void InitEvent(Event *pEvent)
{
    *pEvent = g_cond_new();
}

static void FreeEvent(Event event)
{
    g_cond_free(event);
}

static void InitManualEvent(ManualEvent *pME)
{
    ManualEvent pNewME = malloc(sizeof(*pNewME));
    pNewME->cond = g_cond_new();
    pNewME->signalled = FALSE;
    *pME = pNewME;
}

static void FreeManualEvent(ManualEvent ME)
{
    g_cond_free(ME->cond);
    free(ME);
}

static void WaitForManualEvent(ManualEvent ME)
{
	GTimeVal tv;
	multi_debug("wait for manual event locks");
	g_mutex_lock(condMutex);
	while (!ME->signalled) {
		multi_debug("waiting for manual event");
		g_get_current_time(&tv);
		g_time_val_add(&tv, 10 * 1000 * 1000);
		if (g_cond_timed_wait(ME->cond, condMutex, &tv))
			break;
		else
		{
			multi_debug("still waiting for manual event");
		}
	}

	g_mutex_unlock(condMutex);
	multi_debug("wait for manual event unlocks");
}

static void ResetManualEvent(ManualEvent ME)
{
	multi_debug("reset manual event locks");
    g_mutex_lock(condMutex);
    ME->signalled = FALSE;
    g_mutex_unlock(condMutex);
	multi_debug("reset manual event unlocks");
}

static void SetManualEvent(ManualEvent ME)
{
	multi_debug("reset manual event locks");
    g_mutex_lock(condMutex);
    ME->signalled = TRUE;
    g_cond_broadcast(ME->cond);
    g_mutex_unlock(condMutex);
	multi_debug("reset manual event unlocks");
}

static void InitMutex(Mutex *pMutex)
{
    *pMutex = g_mutex_new();
}

static void FreeMutex(Mutex mutex)
{
    g_mutex_free(mutex);
}

#define Mutex_Lock(mutex) g_mutex_lock(mutex)
#define Mutex_Release(mutex) g_mutex_unlock(mutex)

#else    /* win32 */

static void TLSCreate(TLSItem *ppItem)
{
    *ppItem = TlsAlloc();
	if (*ppItem == TLS_OUT_OF_INDEXES)
		PrintSystemError("calling TlsAlloc");
}

static void TLSFree(TLSItem pItem)
{
	free(TlsGetValue(pItem));
	TlsFree(pItem);
}

static void TLSSetValue(TLSItem pItem, int value)
{
	int *pNew = (int*)malloc(sizeof(int));
	*pNew = value;
    if (TlsSetValue(pItem, pNew) == 0)
		PrintSystemError("calling TLSSetValue");
}

#define TLSGet(item) *((int*)TlsGetValue(item))

static void InitEvent(Event *pEvent)
{
    *pEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (*pEvent == NULL)
		PrintSystemError("creating event");
}

static void FreeEvent(Event event)
{
    CloseHandle(event);
}

static void InitManualEvent(ManualEvent *pME)
{
    *pME = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (*pME == NULL)
		PrintSystemError("creating manual event");
}

static void FreeManualEvent(ManualEvent ME)
{
    CloseHandle(ME);
}

#define WaitForManualEvent(ME) WaitForSingleObject(ME, INFINITE)
#define ResetManualEvent(ME) ResetEvent(ME)
#define SetManualEvent(ME) SetEvent(ME)

static void InitMutex(Mutex *pMutex)
{
    *pMutex = CreateMutex(NULL, FALSE, NULL);
	if (*pMutex == NULL)
		PrintSystemError("creating mutex");
}

static void FreeMutex(Mutex mutex)
{
    CloseHandle(mutex);
}

#define Mutex_Lock(mutex) WaitForSingleObject(mutex, INFINITE)
#define Mutex_Release(mutex) ReleaseMutex(mutex)

#endif


extern unsigned int MT_GetNumThreads(void)
{
    return numThreads;
}

extern int MT_Enabled(void)
{
    return TRUE;
}

static void MT_CloseThreads(void)
{
    mt_add_tasks(numThreads, TT_CLOSE, NULL);
    if (MT_WaitForTasks(NULL, 0) != (int)numThreads)
		g_print("Error closing threads!\n");
}

static void MT_TaskDone(Task *pt)
{
    MT_SafeInc(&td.doneTasks);
#ifndef GLIB_THREADS
	if (td.doneTasks >= td.totalTasks)
		SetEvent(td.alldone);
#endif

    if (pt)
    {
        free(pt->pLinkedTask);
        free(pt);
    }
}

static Task *MT_GetTask(void)
{
    Task *task = NULL;
    if (g_list_length(td.tasks) > 0)
    {
        task = (Task*)g_list_first(td.tasks)->data;
        td.tasks = g_list_delete_link(td.tasks, g_list_first(td.tasks));
        if (g_list_length(td.tasks) == 0)
        {
            ResetManualEvent(td.activity);
        }
    }
    return task;
}

static void MT_AbortTasks(void)
{
    Task *task;
    multi_debug("abort tasks: lock");
    Mutex_Lock(td.queueLock);

    /* Remove tasks from list */
    while ((task = MT_GetTask()) != NULL)
        MT_TaskDone(task);

    Mutex_Release(td.queueLock);
    multi_debug("abort tasks: release");
}

static int MT_DoTask(void)
{
    int alive = TRUE;
    Task *task;

    multi_debug("do tasks: lock");
    Mutex_Lock(td.queueLock);
    task = MT_GetTask();
    Mutex_Release(td.queueLock);
    multi_debug("do tasks: release");

    if (task)
    {
        switch (task->type)
        {
        case TT_ANALYSEMOVE:
        {
            float doubleError;
            AnalyseMoveTask *amt;
AnalyzeDoubleDecison:
            amt = (AnalyseMoveTask *)task;
            if (AnalyzeMove (amt->pmr, &amt->ms, amt->plGame, amt->psc,
                        &esAnalysisChequer,
                        &esAnalysisCube, aamfAnalysis,
                        afAnalysePlayers, &doubleError ) < 0 )
            {
                MT_AbortTasks();
                td.result = -1;
            }
            if (task->pLinkedTask)
            {    /* Need to analyze take/drop decision in sequence */
                task = task->pLinkedTask;
                goto AnalyzeDoubleDecison;
            }
            break;
        }
        case TT_ROLLOUTLOOP:
            RolloutLoopMT();
            break;
		case TT_RUNCALIBRATIONEVALS:
			RunEvals();
			break;
		case TT_ASYNCTASK:
		{
			AsyncTask *asynctask = (AsyncTask *)task;
			td.result = asynctask->fun(asynctask->data);
			break;
		}
        case TT_TEST:
            printf("Test: waiting for a second");
            g_usleep(1000 * 1000);
            break;
        case TT_CLOSE:
			MT_SafeInc(&td.result);
            alive = FALSE;
            break;
        }
	multi_debug("task done: lock");
        Mutex_Lock(td.queueLock);
        MT_TaskDone(task);
        Mutex_Release(td.queueLock);
	multi_debug("task done: release");
        return alive;
    }
    else
        return -1;
}

#if GCC_ALIGN_HACK

static unsigned int MT_ActualWorkerThreadFunction(void *id);
#ifdef GLIB_THREADS
static gpointer
#else
static void 
#endif
MT_WorkerThreadFunction(gpointer id)
{    /* Align stack and call actual function */
	asm  __volatile__  ("andl $-16, %%esp" : : : "%esp");
	MT_ActualWorkerThreadFunction(id);
#ifdef GLIB_THREADS
return NULL;
#endif
}

unsigned int MT_ActualWorkerThreadFunction(void *id)
#else
#ifdef GLIB_THREADS
static gpointer
#else
static unsigned int
#endif
MT_WorkerThreadFunction(void *id)
#endif
{
	int *pID = (int*)id;
    TLSSetValue(td.tlsItem, *pID);
    free(pID);
	MT_SafeInc(&td.result);
    MT_TaskDone(NULL);    /* Thread created */
    do
    {
        WaitForManualEvent(td.activity);
    } while (MT_DoTask());

#ifdef GLIB_THREADS
	g_usleep(0);	/* Avoid odd crash */
#endif

	return 0;
}

static gboolean WaitForAllTasks(int time)
{
#ifndef GLIB_THREADS
	if (WaitForSingleObject(td.alldone, time) != WAIT_TIMEOUT)
#else
	int j=0;
	while (td.doneTasks != td.totalTasks && j++ < 10)
		g_usleep(100*time);
	if (td.doneTasks == td.totalTasks)
#endif
	{
		td.doneTasks = td.totalTasks = td.addedTasks = 0;
		return TRUE;
	}
	else
		return FALSE;	/* Not done yet */
}

static void WaitingForThreads(void)
{	/* Unlikely to be called */
	multi_debug("Waiting for threads to be created!");
}

static void MT_CreateThreads(void)
{
    unsigned int i;
	td.addedTasks = td.totalTasks = numThreads;
	td.result = 0;
    for (i = 0; i < numThreads; i++)
    {
    	int *pID = (int*)malloc(sizeof(int));
    	*pID = i;
#ifdef GLIB_THREADS
        if (!g_thread_create(MT_WorkerThreadFunction, pID, FALSE, NULL))
#else
        if (_beginthread(MT_WorkerThreadFunction, 0, pID) == 0)
#endif
            printf("Failed to create thread\n");
    }
	/* Wait for all the threads to be created (timeout after 2 seconds) */
	if (MT_WaitForTasks(WaitingForThreads, 1000) != (int)numThreads)
		g_print("Error creating threads!\n");
}

void MT_SetNumThreads(unsigned int num)
{
	if (num != numThreads)
	{
		MT_CloseThreads();
		numThreads = num;
		MT_CreateThreads();
	}
}

extern void MT_InitThreads(void)
{
#ifdef GLIB_THREADS
	if (!g_thread_supported ())
		g_thread_init (NULL);
	g_assert(g_thread_supported());
#endif

    td.tasks = NULL;
	td.doneTasks = td.totalTasks = td.addedTasks = 0;
    InitManualEvent(&td.activity);
    TLSCreate(&td.tlsItem);
	TLSSetValue(td.tlsItem, 0);	/* Main thread shares id 0 */
    InitEvent(&td.alldone);
    InitMutex(&td.multiLock);
    InitMutex(&td.queueLock);
	InitManualEvent(&td.syncStart);
	InitManualEvent(&td.syncEnd);
#ifdef GLIB_THREADS
    if (condMutex == NULL)
        condMutex = g_mutex_new();
#endif
}

extern void MT_StartThreads(void)
{
    if (numThreads == 0)
	{
#if TRY_COUNTING_PROCEESING_UNITS
        numThreads = GetLogicalProcssingUnitCount();
#else
        numThreads = 1;
#endif
		MT_CreateThreads();
	}
}

void MT_AddTask(Task *pt, gboolean lock)
{
	if (lock)
	{
		multi_debug("add task: lock");
		Mutex_Lock(td.queueLock);
	}
	td.addedTasks++;
	td.tasks = g_list_append(td.tasks, pt);
	if (g_list_length(td.tasks) == 1)
	{    /* First task */
		td.result = 0;
		SetManualEvent(td.activity);
	}
	if (lock)
	{
		Mutex_Release(td.queueLock);
		multi_debug("add task: release");
	}
}

void mt_add_tasks(int num_tasks, TaskType tt, gpointer linked)
{
	int i;
	multi_debug("add many tasks: lock");
	Mutex_Lock(td.queueLock);
	td.totalTasks = num_tasks;
	for (i = 0; i < num_tasks; i++)
	{
		Task *pt = (Task*)malloc(sizeof(Task));
		pt->type = tt;
		pt->pLinkedTask = linked;
		MT_AddTask((Task*)pt, FALSE);
	}
	Mutex_Release(td.queueLock);
	multi_debug("add many release: lock");
}

int MT_WaitForTasks(void (*pCallback)(void), int callbackTime)
{
    int callbackLoops = callbackTime / UI_UPDATETIME;
    int waits = 0;

	if (td.addedTasks == 0)
		return 0;

    multi_debug("wait for tasks: lock(1)");
    Mutex_Lock(td.queueLock);
    td.totalTasks = td.addedTasks;
    Mutex_Release(td.queueLock);
    multi_debug("wait for tasks: release(1)");

	multi_debug("while waiting for all tasks");
    while (!WaitForAllTasks(UI_UPDATETIME))
    {
        waits++;
        if (pCallback && waits >= callbackLoops)
        {
            waits = 0;
            pCallback();
        }

#if USE_GTK
        SuspendInput();
        while(gtk_events_pending())
			gtk_main_iteration();
        ResumeInput();
#endif
    }
	multi_debug("done while waiting for all tasks");

    return td.result;
}

extern void MT_Close(void)
{
    MT_CloseThreads();

    FreeManualEvent(td.activity);
    FreeEvent(td.alldone);
    FreeMutex(td.multiLock);

    /* queueLock is locked around MT_TaskDone and may not be released in time
     * unless we wait for it here before we free the mutex */
    Mutex_Lock(td.queueLock);
    Mutex_Release(td.queueLock);
    FreeMutex(td.queueLock);

    FreeManualEvent(td.syncStart);
    FreeManualEvent(td.syncEnd);
    TLSFree(td.tlsItem);
}

extern int MT_GetThreadID(void)
{
    return TLSGet(td.tlsItem);
}

extern void MT_Exclusive(void)
{
	Mutex_Lock(td.multiLock);
}

extern void MT_Release(void)
{
	Mutex_Release(td.multiLock);
}

extern int MT_GetDoneTasks(void)
{
    return td.doneTasks;
}

extern void MT_SyncInit(void)
{
	ResetManualEvent(td.syncStart);
	ResetManualEvent(td.syncEnd);
}

extern void MT_SyncStart(void)
{
	static int count = 0;

	/* Wait for all threads to get here */
	if (MT_SafeIncValue(&count) == (int)numThreads)
	{
		count--;
		start = get_time();
		SetManualEvent(td.syncStart);
	}
	else
	{
		WaitForManualEvent(td.syncStart);
		if (MT_SafeDecCheck(&count))
			ResetManualEvent(td.syncStart);
	}
}

extern double MT_SyncEnd(void)
{
	static int count = 0;
	double now;

	/* Wait for all threads to get here */
	if (MT_SafeIncValue(&count) == (int)numThreads)
	{
		now = get_time();
		count--;
		SetManualEvent(td.syncEnd);
		return now - start;
	}
	else
	{
		WaitForManualEvent(td.syncEnd);
		if (MT_SafeDecCheck(&count))
			ResetManualEvent(td.syncEnd);
		return 0;
	}
}
#else
/* Avoid no code warning */
extern int dummy;
#endif
