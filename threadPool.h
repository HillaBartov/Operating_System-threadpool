//Hilla Bartov 315636779 LATE-SUBMISSION
#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <sys/types.h>
#include <stdbool.h>
#include "osqueue.h"

typedef void (*taskFunc)(void *arg);

typedef struct thread_pool {
    pthread_t *threads;
    int numberOfThreads;
    int workingThreads;
    OSQueue *tasks;
    bool shouldStop;
    bool destroyCalled;
    pthread_mutex_t mutex;
    pthread_cond_t condTask;
    pthread_cond_t condThread;


} ThreadPool;
typedef struct task {
    taskFunc func;
    void *arg;
} Task;

ThreadPool *tpCreate(int numOfThreads);

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param);

void *executeTasks(void *threadPool);

int waitForTask(ThreadPool *threadPool);

void destroyError(ThreadPool *tp);

#endif
