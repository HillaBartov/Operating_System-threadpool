//Hilla Bartov 315636779 LATE-SUBMISSION
#include "threadPool.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

ThreadPool *tpCreate(int numOfThreads) {
    ThreadPool *pool = malloc(sizeof *pool);
    if (pool == NULL) {
        perror("Error in malloc: ");
        exit(1);
    }
    pthread_t tp[numOfThreads];
    pool->tasks = osCreateQueue();
    pool->shouldStop = false;
    pool->destroyCalled = false;
    if (pthread_mutex_init(&(pool->mutex), NULL) != 0) {
        osDestroyQueue(pool->tasks);
        free(pool);
        perror("Error in pthread_: ");
        exit(1);
    }
    if (pthread_cond_init(&(pool->condTask), NULL) != 0) {
        pthread_mutex_destroy(&(pool->mutex));
        osDestroyQueue(pool->tasks);
        free(pool);
        perror("Error in pthread_: ");
        exit(1);
    }
    if (pthread_cond_init(&(pool->condThread), NULL) != 0) {
        pthread_cond_destroy(&(pool->condTask));
        pthread_mutex_destroy(&(pool->mutex));
        osDestroyQueue(pool->tasks);
        free(pool);
        perror("Error in pthread_: ");
        exit(1);
    }
    pool->threads = tp;
    pool->numberOfThreads = numOfThreads;
    pool->workingThreads = 0;
    int i;
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, executeTasks, pool) != 0) {
            pthread_cond_destroy(&(pool->condTask));
            pthread_mutex_destroy(&(pool->mutex));
            pthread_cond_destroy(&(pool->condThread));
            osDestroyQueue(pool->tasks);
            free(pool);
            perror("Error in system call: ");
            exit(1);
        }
    }

    //clear all threads when they are done
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_detach(pool->threads[i]) != 0) {
            pthread_cond_destroy(&(pool->condTask));
            pthread_mutex_destroy(&(pool->mutex));
            pthread_cond_destroy(&(pool->condThread));
            osDestroyQueue(pool->tasks);
            free(pool);
            perror("Error in system call: ");
            exit(1);
        }
    }
    return pool;
}

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {
    threadPool->destroyCalled = true;
    if (shouldWaitForTasks) {
        while (!osIsQueueEmpty(threadPool->tasks));
    } else {
        Task *task;
        while ((task = osDequeue(threadPool->tasks)) != NULL) {
            free(task);
        }
    }
    threadPool->shouldStop = true;
    if (pthread_cond_broadcast(&(threadPool->condTask)) != 0) {
        destroyError(threadPool);
    }
    //wait for all the threads to return
    while (threadPool->numberOfThreads != 0) {
        if (pthread_cond_wait(&(threadPool->condThread), &(threadPool->mutex)) != 0) {
            destroyError(threadPool);
        }
    }
    if (pthread_cond_destroy(&(threadPool->condTask)) != 0) {
        destroyError(threadPool);
    }
    if (pthread_cond_destroy(&(threadPool->condThread)) != 0) {
        destroyError(threadPool);
    }
    if (pthread_mutex_destroy(&(threadPool->mutex)) != 0) {
        destroyError(threadPool);
    }
    osDestroyQueue(threadPool->tasks);
    free(threadPool);
}

void destroyError(ThreadPool *tp) {
    osDestroyQueue(tp->tasks);
    free(tp);
    perror("Error in pthread_: ");
    exit(1);
}

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {
    if (threadPool->destroyCalled) {
        return -1;
    }
    Task *newTask = malloc(sizeof *newTask);
    newTask->func = computeFunc;
    newTask->arg = param;
    osEnqueue(threadPool->tasks, newTask);
    if (pthread_cond_broadcast(&(threadPool->condTask)) != 0) {
        tpDestroy(threadPool, 0);
        perror("Error in pthread_: ");
        exit(1);
    }
    return 0;
}

void *executeTasks(void *pool) {
    ThreadPool *threadPool = (ThreadPool *) pool;
    Task *task;
    while (waitForTask(threadPool)) {
        //when task is given and destroy has'nt called yet
        threadPool->workingThreads++;
        if (pthread_mutex_unlock(&(threadPool->mutex)) != 0) {
            tpDestroy(threadPool, 0);
            perror("Error in pthread_: ");
            exit(1);
        }
        if ((task = osDequeue(threadPool->tasks)) != NULL) {
            (task->func)(task->arg);
            free(task);
        }
        //when thread executed task, it's no longer working.
        if (pthread_mutex_lock(&(threadPool->mutex)) != 0) {
            tpDestroy(threadPool, 0);
            perror("Error in pthread_: ");
            exit(1);
        }
        threadPool->workingThreads--;
        if (pthread_mutex_unlock(&(threadPool->mutex)) != 0) {
            tpDestroy(threadPool, 0);
            perror("Error in pthread_: ");
            exit(1);
        }
    }
    //end thread
    return NULL;
}

int waitForTask(ThreadPool *threadPool) {
    if (pthread_mutex_lock(&(threadPool->mutex)) != 0) {
        tpDestroy(threadPool, 0);
        perror("Error in pthread_: ");
        exit(1);
    }
    //when queue tasks is empty but pool's destroy function hasn't called yet, wait for a task
    while (osIsQueueEmpty(threadPool->tasks) && !threadPool->shouldStop) {
        if (pthread_cond_wait(&(threadPool->condTask), &(threadPool->mutex)) != 0) {
            tpDestroy(threadPool, 0);
            perror("Error in pthread_: ");
            exit(1);
        }
    }
    //when destroy function is called
    if (threadPool->shouldStop) {
        threadPool->numberOfThreads--;
        if (pthread_cond_signal(&(threadPool->condThread)) != 0) {
            tpDestroy(threadPool, 0);
            perror("Error in pthread_: ");
            exit(1);
        }
        if (pthread_mutex_unlock(&(threadPool->mutex)) != 0) {
            tpDestroy(threadPool, 0);
            perror("Error in pthread_: ");
            exit(1);
        }
        //stop thread loop in executeTasks
        return 0;
    }
    return 1;
}
