/**
 * author: Jiaqi Cao
 * password_cracker
 * CS 241 - Spring 2021
 */

#include "cracker1.h"
#include "format.h"
#include "utils.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


#include "./includes/queue.h"
#include <math.h>
#include <crypt.h>
#include <string.h>

typedef struct _job_t {
  char *user;
  char *password_h;
  char *passwords;
} job_t;

static job_t *this_task = NULL;

pthread_barrier_t barrier;
pthread_mutex_t m_h = PTHREAD_MUTEX_INITIALIZER;

static int done = 0;
static int found = 0;
static int all = 0;
static size_t countthread = 0;
static char* pwd = NULL;


void *helper(void* input);

int start(size_t thread_count) {
    // TODO your code here, make sure to use thread_count!
    // Remember to ONLY crack passwords in other threads
    pthread_barrier_init(&barrier, NULL, thread_count+1);
    countthread = thread_count;
    pthread_t tid[thread_count];
    for (size_t i = 0; i < thread_count; i++){
        pthread_create(tid+i, NULL, helper, (void*)(i+1));
    }
    this_task = calloc(sizeof(job_t),1);
    this_task->user = calloc(9, sizeof(char));
    this_task->password_h = calloc(14, sizeof(char));
    this_task->passwords = calloc(9,sizeof(char));
    pwd = calloc(9,sizeof(char));
    char *buf = NULL;
    size_t size = 0;
    ssize_t read = 0;
    while (1){   
        read = getline(&buf, &size, stdin);
        if(read == -1){
            done = 1;
            free(this_task->password_h);
            free(this_task->passwords);
            free(this_task->user);
            free(this_task);
            free(pwd);
            free(buf);
            pthread_barrier_wait(&barrier);
            break;
        }
        if (buf[read-1] == '\n') {
            buf[read-1] = '\0';
        }
        char *user = strtok(buf, " ");
        char *hash = strtok(NULL, " ");
        char *passwords = strtok(NULL, " ");
        strcpy(this_task->user, user);
        strcpy(this_task->password_h, hash);
        strcpy(this_task->passwords, passwords);
        v2_print_start_user(this_task->user);

        double start = getTime();
        double start_cpu = getCPUTime();
        pthread_barrier_wait(&barrier);
        pthread_barrier_wait(&barrier);   
        pthread_barrier_wait(&barrier);
        
        double duration = getTime() - start;
        double duration_cpu = getCPUTime() - start_cpu;

        if(found){
            v2_print_summary(this_task->user, pwd, all,duration, duration_cpu, 0);  
        }else{
            v2_print_summary(this_task->user, NULL, all, duration, duration_cpu, 1);
        }
        found = 0;
        all = 0;
    }
    for (size_t j = 0; j < thread_count; j++) {
      pthread_join(tid[j], NULL);
    }

    pthread_mutex_destroy(&m_h);
    pthread_barrier_destroy(&barrier);
    return 0; // DO NOT change the return code since AG uses it to check if your
              // program exited normally
}

void *helper(void* input){
     int tid = (long) input;
    struct crypt_data cdata;
    cdata.initialized = 0;
    char *password = calloc(10, sizeof(char));
    while(1){
        pthread_barrier_wait(&barrier);
        if(done) break;
        strcpy(password, this_task->passwords);
        int count_t = 0;
        long idx = 0;
        long count = 0;
        int left = strlen(password) - getPrefixLength(password);
        getSubrange(left, countthread, tid, &idx, &count);
        setStringPosition(password + getPrefixLength(password), idx);
        v2_print_thread_start(tid, this_task->user, idx, password);
        long i = 0;
        for (; i < count; i++){
            char *password_h_v = crypt_r(password, "xx", &cdata);
            count_t++;
            if(!strcmp(password_h_v, this_task->password_h)){
                pthread_mutex_lock(&m_h);
                strcpy(pwd, password);
                found = 1;
                v2_print_thread_result(tid, count_t, 0);
                all = all + count_t;
                pthread_mutex_unlock(&m_h);
                break;
            }
            if(found){
                pthread_mutex_lock(&m_h);
                v2_print_thread_result(tid, count_t, 1);
                all = all + count_t;
                pthread_mutex_unlock(&m_h);
                break;
            }
            incrementString(password);
        }

        pthread_barrier_wait(&barrier);
        if(!found){
            pthread_mutex_lock(&m_h);
            v2_print_thread_result(tid, count_t, 2);
            all = all + count_t;
            pthread_mutex_unlock(&m_h);
        }
        pthread_barrier_wait(&barrier);
    }
    free(password);
    return NULL;
}



