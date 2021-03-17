#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <usloss.h>
#include <phase1.h>

#include "phase2Int.h"

typedef struct Sleeper{
    int WAKEUPwakeup; // Chop Suey!
    int timeGoSleep; // when the process went to sleep
    int pid; // process of id that went to sleep
    int conditionId; // what is put to sleep
    Sleeper *next;
} Sleeper;

static int      ClockDriver(void *);
static void     SleepStub(USLOSS_Sysargs *sysargs);

static int      now; // current time
static int      clockPid;
static int      lockId;

Sleeper *head;

/*
 * P2ClockInit
 *
 * Initialize the clock data structures and fork the clock driver.
 */
void P2ClockInit(void) {
    int rc;

    P2ProcInit();

    // initialize data structures here
    head = (Sleeper *) malloc(sizeof(Sleeper));
    head->next = NULL;
    head->pid = -1;
    head->WAKEUPwakeup = -1; // Chop Suey!
    head->timeGoSleep = -1;
    head->conditionId = -1;

    rc = P1_LockCreate("lock", &lockId);
    assert(rc == P1_SUCCESS);

    rc = P2_SetSyscallHandler(SYS_SLEEP, SleepStub);
    assert(rc == P1_SUCCESS);

    // fork the clock driver here
    rc = P1_Fork("clock", ClockDriver, NULL, P1_MAXSTACK, 1, clockPid);
    assert(rc == P1_SUCCESS);
}

/*
 * P2ClockShutdown
 *
 * Clean up the clock data structures and stop the clock driver.
 */

void P2ClockShutdown(void) {
    // stop clock driver
}

/*
 * ClockDriver
 *
 * Kernel process that manages the clock device and wakes sleeping processes.
 */
static int ClockDriver(void *arg) {

    while(1) {
        int rc;
        Sleeper *temp;

        // wait for the next interrupt
        rc = P1_DeviceWait(USLOSS_CLOCK_DEV, 0, &now);
        if (rc == P1_WAIT_ABORTED) {
            break;
        }
        assert(rc == P1_SUCCESS);

        // wakeup any sleeping processes whose wakeup time has arrived
        temp = head->next;
        // NOTE: WAKEUP confusion?
        while(temp != NULL || temp.WAKEUPwakeup <= now){ // Chop Suey!
            rc = P1_Signal(temp->conditionId);
            assert(rc == P1_SUCCESS);
            head->next = temp->next;
            temp = head->next;
        }
    }
    return P1_SUCCESS;
}

/*
 * P2_Sleep
 *
 * Causes the current process to sleep for the specified number of seconds.
 */
int P2_Sleep(int seconds) {
    int wakeupTime;
    int rc;
    Sleeper *new;
    Sleeper *temp;
    // update current time and determine wakeup time
    int status = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &now); // NOTE: 0 for second param may break
    if (status != USLOSS_DEV_OK) {
        USLOSS_Halt(status);
    }
    wakeupTime = now + (seconds * 1000);

    // add current process to data structure of sleepers
    new = (Sleeper *) malloc(sizeof(Sleeper));
    new->WAKEUPwakeup = wakeupTime; // Chop Suey!
    new->timeGoSleep = now;
    new->pid = P1_GetPid();
    rc = P1_CondCreate((toString(new->pid)), lockId, &new->conditionId); // NOTE: Pointer problem maybe
    assert(rc == P1_SUCCESS);

    // add sleeper to list
    temp = head;
    while(temp->next != NULL || temp->next->WAKEUPwakeup <= new->WAKEUPwakeup ){ // chop Suey
        temp = temp->next;
    }
    new->next = temp->next
    temp->next = new;
    
    // wait until it's wakeup time
    rc = P1_Wait(new->conditionId);
    return P1_SUCCESS;
}

/*
 * SleepStub
 *
 * Stub for the Sys_Sleep system call.
 */
static void SleepStub(USLOSS_Sysargs *sysargs) {
    int seconds = (int) sysargs->arg1;
    int rc = P2_Sleep(seconds);
    sysargs->arg4 = (void *) rc;
}

