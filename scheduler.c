#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "linked_list.h"

/*  
    Copyright (C) 2013 Jason Giancono (jasongiancono@gmail.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/


/*This is the max input the scheduler will handle*/
#define MAXPROCESS 100
typedef enum    { CPU, IO }   STATE;

typedef struct {
    int    pid;
    int    ac;
    int    arrive;
    STATE  state;
    int remaining;
} PA;
/*Job task is the format of future tasks a PID will have. The actually PID is indicated by where in the array it is*/
typedef struct {
    int    ac;
    STATE  state;
    int time;
} JOBTASK;

/*this is the ready queue*/
LinkedList* cpuQueue;

/*this is the i/o queue*/
LinkedList* ioQueue;

/*these values are shared between threads, but only are read only outside the owners thread
This means the 'owner' can read the value without grabbing a mutex, but writing needs it and
reading (if you aren't the 'owner' needs it too)*/
int cpuTime=0;
int ioTime=0;

/*this is the lock for time*/
pthread_mutex_t time_mutex;

/*this sends a signal whenever time is updated (or time is
'frozen' (which happens when the queue is empty)*/
pthread_cond_t time_cv;

/*these values are read and written by both threads. A thread needs a lock to edit or read
these values*/
int ioCount = 0;
int cpuCount = 0;

/*this is the lock for the queue counts*/
pthread_mutex_t count_mutex;
pthread_cond_t count_threshold_cv;

/*these values are not shared between threads*/
int ioBusy=0;
int cpuBusy=0;
int ioWait=0;
int cpuWait=0;




/*this is where all the future activities are kept for each PID*/
LinkedList* processes[MAXPROCESS];

/*function that writes an entry to a log file given
its name, the heading and a pa to write.*/
void writeEntry(char* logName, char* heading, PA* entry)
{
    FILE* f;
    char stuff[4];
    f = fopen(logName, "a");
    if (f != NULL)
    {
        (entry->state==CPU) ? strcpy(stuff,"CPU") : strcpy(stuff,"I/O");
        fprintf(f, "%s\nPID=%d\nAC=%d\nState=%s\nArrive=%d\nTime=%d\n\n",heading, entry->pid, entry->ac, stuff, entry->arrive, entry->remaining);
        fclose(f);
    }
    else perror("error writing log");
}
/*this is the cpu thread*/
void * cpu(void *ptr)
{
    /*variable for telling if there are any processes left to execute*/
    int notdone = 1;
    do
    {
        /*check to see if there is anything in the queue (Critical Section)*/
        pthread_mutex_lock(&count_mutex);
        /*if there isn't anything in the queue, signal the other thread that
        it can go ahead and write any logs it's waiting to write. Then wait
        for a signal from the other thread that there is processes in the
        queue*/
        while(cpuCount==0)
        {
            pthread_cond_signal(&time_cv);
            pthread_cond_wait(&count_threshold_cv, &count_mutex);
        }
        pthread_mutex_unlock(&count_mutex);
        /*grab next item from queue (Remainder section)*/
        PA* pa = returnFirstElement(cpuQueue);
        /*check to see if the arrive time of the next process
        is in the future compared to the current time. If it is
        this means the CPU just finished waiting and hasn't been
        updating its time, which means we update the time with the
        arrival time (becaues the queue would have had to be empty
        when the process arrived. We also need to lock the time mutex
        and signal the time cond because we are updating the time*/
        if(cpuTime<pa->arrive)
        {
            pthread_mutex_lock(&time_mutex);
            cpuTime = pa->arrive;
            pthread_mutex_unlock(&time_mutex);
            pthread_cond_signal(&time_cv);
        }
        /*update the total waiting time experienced*/
        cpuWait= cpuWait+cpuTime-pa->arrive;
        /**********************************************************
        -------------------------------------------------------
        if this wasn't a simulation, right here would be where
        the actual burst happens. Because we are in a simulation
        then nothing happens but lets pretend :)
        --------------------------------------------------------
        ***********************************************************/
        /*we now update the cpu clock to the time it would be after
        the burst, this requires a lock on the time mutex and signal time
        incase a log is waiting to be written*/
        pthread_mutex_lock(&time_mutex);
        cpuTime += pa->remaining;
        pthread_mutex_unlock(&time_mutex);
        pthread_cond_signal(&time_cv);
        /*updates the time total time the cpu has been in use
        for the end stats*/
        cpuBusy += pa->remaining;
        /*this block runs if the process is now finished*/
        if (returnFirstElement(processes[pa->pid-1]) == NULL)
        {
            /*write to the log. Doesn't need mutex because only
            this thread (and the main thread) writes to log-B.
            But the main thread won't be executing right now.*/
            char message[31];
            sprintf(message,"Process PID-%d is terminated.",pa->pid);
            writeEntry("log-B", message, pa);
            /*remove the queue for that process*/
            freeList(processes[pa->pid-1]);
            /*remove the process from the cpu queue*/
            removeFirst(cpuQueue);
            /*update the queue count for CPU, need lock on count*/
            pthread_mutex_lock(&count_mutex);
            cpuCount--;
            /*test to see if there is any processes left in either
            queue to see if we need to exit. Note that we still have
            the lock. if yes then signal IO so it can exit too.*/
            if((cpuCount==0)&&(ioCount==0))
            {
                pthread_mutex_unlock(&count_mutex);
                pthread_cond_signal(&count_threshold_cv);
                notdone = 0;
            }
            else pthread_mutex_unlock(&count_mutex);
        }
        /*this block runs if there is more bursts for the process
        to run before it exits*/
        else
        {
            /*grab the next activity*/
            JOBTASK *nextJob = returnFirstElement(processes[pa->pid-1]);
            /*set the remaining time for next activity*/
            pa->remaining = nextJob->time;
            /*remove the next activity from the queue*/
            removeFirst(processes[pa->pid-1]);
            /*we don't really need to grab these from the nextJob,
            but we COULD!*/
            pa->ac++;
            pa->state = IO;
            pa->arrive = cpuTime;
            /*this syncronizes the two threads in respect to time.
            Basically the program was working fine but it would write
            the logs out of order. In order to combat that, before it
            writes to the log and puts the process in the other threads
            queue, we wait for either the other threads time to be equal/greater
            to the time in this thread OR for the other thread to be waiting for
            something else in the queue. This makes sure a process doesn't
            'time travel' in this simulation. It wouldn't matter in practice
            because you would have a hardware CPU clock that couldn't be edited*/
            pthread_mutex_lock(&time_mutex);
            pthread_mutex_lock(&count_mutex);
            while ((ioTime < cpuTime) && (ioCount>0))
            {
                pthread_mutex_unlock(&count_mutex);
                pthread_cond_wait(&time_cv,&time_mutex);
                pthread_mutex_lock(&count_mutex);
            }
            /*Now we know there isn't any time travel happening,
            we can write the log*/
            char message[] = "Finishing CPU Activity.";
            writeEntry("log-A", message, pa);
            pthread_mutex_unlock(&time_mutex);
            pthread_mutex_unlock(&count_mutex);
            /*take the process out of the CPU queue
            NOTE: soft means it doesn't free the memory*/
            softRemoveFirst(cpuQueue);
            /*put the process in the ioQueue*/
            insertLast(ioQueue,pa);
            /*update the counters and signal the other
            process if it was empty*/
            pthread_mutex_lock(&count_mutex);
            cpuCount--;
            ioCount++;
            pthread_mutex_unlock(&count_mutex);
            /*signal the other process that it now has things
            in it's queue (it will only pick up the signal if
            it was empty*/
            pthread_cond_signal(&count_threshold_cv);
        }
    }
    while(notdone);
    pthread_exit(NULL);
}
/*this is the io thread*/
void * io(void *ptr)
{
    /*variable for telling if there are any processes left to execute*/
    int notdone = 1;
    do
    {
        /*check to see if there is anything in the queue (Critical Section)*/
        pthread_mutex_lock(&count_mutex);
        /*if there isn't anything in the queue, signal the other thread that
        it can go ahead and write any logs it's waiting to write. Then wait
        for a signal from the other thread that there is processes in the
        queue or there isn't any processes left at all (which means exit)*/
        while ((ioCount==0) && (cpuCount>0))
        {
            pthread_cond_signal(&time_cv);
            pthread_cond_wait(&count_threshold_cv, &count_mutex);
        }
        /*check to see if there is anything in the queue. If there
        isn't then there are no processes left in either queue.*/
        if(ioCount>0)
        {
            pthread_mutex_unlock(&count_mutex);
            /*grab next item from queue (Remainder section)*/
            PA* pa = returnFirstElement(ioQueue);
            /*check to see if the arrive time of the next process
            is in the future compared to the current time of IO. If it is
            this means the IO timer just finished waiting and hasn't been
            updating its time, which means we update the time with the
            arrival time (becaues the queue would have had to be empty
            when the process arrived. We also need to lock the time mutex
            and signal the time cond because we are updating the time*/
            if(ioTime<pa->arrive)
            {
                pthread_mutex_lock(&time_mutex);
                ioTime = pa->arrive;
                pthread_mutex_unlock(&time_mutex);
                pthread_cond_signal(&time_cv);
            }
            /*update the total waiting time experienced*/
            ioWait= ioWait+ioTime-pa->arrive;
            /**********************************************************
            -------------------------------------------------------
            if this wasn't a simulation, right here would be where
            the actual burst happens. Because we are in a simulation
            then nothing happens but lets pretend :)
            --------------------------------------------------------
            ***********************************************************/
            /*we now update the io clock to the time it would be after
            the burst, requires a lock on the time mutex and signal time
            incase a log is waiting to be written*/
            pthread_mutex_lock(&time_mutex);
            ioTime += pa->remaining;
            pthread_mutex_unlock(&time_mutex);
            pthread_cond_signal(&time_cv);
            /*updates the time total time the cpu has been in use
            for the end stats*/
            ioBusy += pa->remaining;
            /*grab the next activity*/
            JOBTASK *nextJob = returnFirstElement(processes[pa->pid-1]);
            /*set the remaining time for next activity*/
            pa->remaining = nextJob->time;
            /*remove the next activity from the queue*/
            removeFirst(processes[pa->pid-1]);
            /*we don't need to grab these from the nextJob because they are a given,
            but we COULD!*/
            pa->arrive = ioTime;
            pa->ac++;
            pa->state = CPU;
            /*this syncronizes the two threads in respect to time.
            Basically the program was working fine but it would write
            the logs out of order. In order to combat that, before it
            writes to the log and puts the process in the other threads
            queue, we wait for either the other threads time to be equal/greater
            to the time in this thread OR for the other thread to be waiting for
            something else in the queue. This makes sure a process doesn't
            'time travel' in this simulation. It wouldn't matter in practice
            because you would have a hardware CPU clock that couldn't be edited*/
            pthread_mutex_lock(&time_mutex);
            pthread_mutex_lock(&count_mutex);
            /*the reason for the equals is just for consistancy, otherwise
            when both cpu and io finished an activity at the same time, the one
            that was written first would be random*/
            while ((ioTime >= cpuTime) && (cpuCount>0))
            {
                pthread_mutex_unlock(&count_mutex);
                pthread_cond_wait(&time_cv,&time_mutex);
                pthread_mutex_lock(&count_mutex);
            }
            /*Now we know there isn't any time travel happening,
            we can write the log*/
            char message[] = "Finishing I/O Activity.";
            writeEntry("log-A", message, pa);
            pthread_mutex_unlock(&time_mutex);
            pthread_mutex_unlock(&count_mutex);
            /*take the process out of the IO queue
            NOTE: soft means it doesn't free the memory*/
            softRemoveFirst(ioQueue);
            /*put the process in the ioQueue*/
            insertLast(cpuQueue,pa);
            /*update the counters and signal the other
            process if it was empty*/
            pthread_mutex_lock(&count_mutex);
            cpuCount++;
            ioCount--;
            pthread_mutex_unlock(&count_mutex);
            /*signal the other process that it now has things
            in it's queue (it will only pick up the signal if
            it was empty*/
            pthread_cond_signal(&count_threshold_cv);
        }
        else
        /*this executes when all processes are terminated*/
        {
            pthread_mutex_unlock(&count_mutex);
            notdone = 0;
        }
    }
    while(notdone);
    pthread_exit(NULL);
}

main(int argc, char** argv)
{
    /*check to see if filename is supplied*/
    if (argc==2)
    {
        /*delcarations*/
        FILE *job, *process;
        PA* pa;
        /*open the job list*/
        job = fopen(argv[1], "r");
        if(job !=NULL)
        {
            /*create the queues*/
            cpuQueue = createList();
            ioQueue = createList();
            /*do this until there is not more processes in the file*/
            while(!feof(job))
            {
                /*declare some strings to use*/
                char tempStr[15] = "PID_";
                char pidStr[15];
                char stateStr[4];
                /*allocate a new PA for the process we're about to read*/
                pa = (PA*)(malloc(sizeof(PA)));
                /*read the next line in the job file*/
                fscanf(job,"PID_%d",&(pa->pid));
                /*first activity will be 1*/
                pa->ac = 1;
                /*first activity always gets there at t=1*/
                pa->arrive = 0;
                /*the the PID number*/
                sprintf(pidStr,"%d",pa->pid);
                /*this gives the filename to the PID file*/
                strcat(tempStr, pidStr);
                /*opens the PID file*/
                process = fopen(tempStr, "r");
                if(process ==NULL)
                {
                    perror("failure opening PID file");
                    fflush(stderr);
                }
                /*load the first activity from the file*/
                fscanf(process,"1 %s %d",stateStr,&(pa->remaining));
                fscanf(process,"\n");
                /*set the state*/
                if(!strcmp(stateStr,"CPU"))
                    pa->state = CPU;
                else pa->state = IO;
                /*go to new line*/
                fscanf(job,"\n");
                /*put PID in cpu queue*/
                insertLast(cpuQueue, pa)
                /*write an entry in the log*/;
                char message[] = "New Process:";
                writeEntry("log-A", message, pa);
                int ii;
                int timing;
                /*make a list for all the future activities*/
                processes[pa->pid-1] = createList();
                /*load the future activities into a linked list*/
                while(!feof(process))
                {
                    fscanf(process,"%d %s %d",&ii,stateStr, &timing);
                    JOBTASK *jobs = (JOBTASK*)malloc(sizeof(JOBTASK));
                    jobs->ac = ii;
                    if(!strcmp(stateStr,"CPU"))
                        jobs->state = CPU;
                    else jobs->state = IO;
                    jobs->time = timing;
                    insertLast(processes[pa->pid-1],jobs);
                    fscanf(process,"\n");
                }
                /*close the file*/
                fclose(process);
                /*add to count*/
                cpuCount++;
            }
            /*close the job file*/
            fclose(job);
            /*set the number of total processes (used in stats at the end*/
            int processCount=cpuCount;
            /*create threads*/
            pthread_t cpuThread;
            pthread_t ioThread;
            /*initialise mutexes and condition variables*/
            pthread_mutex_init(&count_mutex, NULL);
            pthread_mutex_init(&time_mutex, NULL);
            pthread_cond_init (&count_threshold_cv, NULL);
            pthread_cond_init (&time_cv, NULL);
            /*set pointers to the functions for the threads*/
            void *(*cpuPtr)(void *) = &cpu;
            void *(*ioPtr)(void *) = &io;
            /*set the attribute for the thread (in this case it is joinable)*/
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
            /*start the threads*/
            pthread_create(&cpuThread,&attr,cpuPtr,NULL);
            pthread_create(&ioThread,&attr,ioPtr,NULL);
            /*destroy the attribute*/
            pthread_attr_destroy(&attr);
            /*wait for both threads to exit*/
            pthread_join(cpuThread, NULL);
            pthread_join(ioThread, NULL);
            /*destroy the mutex and condition variables*/
            pthread_mutex_destroy(&count_mutex);
            pthread_mutex_destroy(&time_mutex);
            pthread_cond_destroy(&count_threshold_cv);
            pthread_cond_destroy(&time_cv);
            float aioWT,acpuWT,cpuU,ioU;
            aioWT = ((float)ioWait)/((float)processCount);
            acpuWT = ((float)cpuWait)/((float)processCount);
            cpuU = (((float)cpuBusy)/((float)cpuTime));
            ioU = (((float)ioBusy)/((float)ioTime));
            /*write end stats to file and print them out for debugging/ease*/
            FILE* f = fopen("log-B","a");
            if (f!=NULL)
                fprintf(f,"Average waiting time in CPU queue: %f\nAverage waiting time in I/O queue: %f\nCPU utilization: %f%%\nI/O utilization: %f%%\n\n",acpuWT,aioWT,cpuU,ioU);
            else perror("could not open log-B file for writing end stats");
            printf("Average waiting time in CPU queue: %f\nAverage waiting time in I/O queue: %f\nCPU utilization: %f%%\nI/O utilization: %f%%\n\n",acpuWT,aioWT,cpuU,ioU);
            /*free the queues*/
            freeList(ioQueue);
            freeList(cpuQueue);
        }
        else    perror("error loading job file check your args");
    }
    else printf("\nwrong args\n");
    return 0;
}


