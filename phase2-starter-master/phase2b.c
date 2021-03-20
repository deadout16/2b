// James Brechtel and Zach Braaten-Schuettpelz
// CSC452 Project 2b
// This program controls the clock

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
    struct Sleeper *next;
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

    rc = P1_LockCreate("lock", &lockId);
    assert(rc == P1_SUCCESS);

    rc = P2_SetSyscallHandler(SYS_SLEEP, SleepStub);
    assert(rc == P1_SUCCESS);

    // fork the clock driver here
    rc = P1_Fork("clock", ClockDriver, NULL, USLOSS_MIN_STACK, 1, &clockPid);
    assert(rc == P1_SUCCESS);
}

/*
 * P2ClockShutdown4
 *
 * Clean up the clock data structures and stop the clock driver.
 */

void P2ClockShutdown(void) {
    // stop clock driver
    int pkThunder = P1_DeviceAbort(USLOSS_CLOCK_DEV, 0);
    assert(pkThunder == P1_SUCCESS);
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
        
         // Lock for global list
        rc = P1_Lock(lockId);
        assert(rc == P1_SUCCESS);
        
        temp = head;
        // NOTE: WAKEUP confusion?
        while(temp != NULL){
            if(temp->WAKEUPwakeup <= now){
                rc = P1_Signal(temp->conditionId);
                assert(rc == P1_SUCCESS);
            }
            temp = temp->next;
            }
        // Unlock
        rc = P1_Unlock(lockId);
        assert(rc == P1_SUCCESS);
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
    Sleeper *prev;
    char* pidStr;
    char k;
    // update current time and determine wakeup time
    int status = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &now);
    if (status != USLOSS_DEV_OK) {
        USLOSS_Halt(status);
    }
    wakeupTime = now + (seconds * 1000000);
    // add current process to data structure of sleepers
    new = (Sleeper *) malloc(sizeof(Sleeper));
    new->next = NULL;
    new->WAKEUPwakeup = wakeupTime; // Chop Suey!
    new->timeGoSleep = now;
    new->pid = P1_GetPid();

    // get the condition variables
    k = new->pid + '0';
    pidStr = &k;
    rc = P1_CondCreate(pidStr, lockId, &new->conditionId);
    assert(rc == P1_SUCCESS);

    //Locks when adding to global list
    rc = P1_Lock(lockId);
    assert(rc == P1_SUCCESS);

    // add sleeper to list
    temp = head;
    prev = NULL;

    // if head is null
    if(head == NULL){
        head = new;
    }
    else{ 
        // while head is not null
        while(NULL != temp){ 
            if(temp->WAKEUPwakeup <= new->WAKEUPwakeup){
                break;
            }
            prev = temp;
            temp = temp->next;
        }
        // if head not null insert
        if(NULL != temp){
            if(prev != NULL){
                prev->next = new;
                new->next = temp;
            }
            else{
                new->next = head;
                head = new;
            }
        }
        else{
            prev->next = new;
        }
    }
    temp = head;

    // wait until it's wakeup time
    while(wakeupTime > now){
        rc = P1_Wait(new->conditionId);
    }

    // delete nodes (used code from James drills)
    temp = head;
    if(new->pid == temp->pid){
        head = temp->next;
    }
    else{
        while(temp->next != NULL){
            if(temp->next->pid == new->pid){
                if(temp->next->next != NULL){
                    temp->next = temp->next->next;
                    break;
                }
                else{
                    temp->next = NULL;
                    break;
                }
            }
            temp = temp->next;
        }
    }

    // Unlock
    rc = P1_Unlock(lockId);
    assert(rc == P1_SUCCESS);
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

