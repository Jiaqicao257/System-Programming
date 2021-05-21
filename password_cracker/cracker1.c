/**
 * author: Jiaqi Cao
 * Password Cracker Lab
 * CS 241 - Spring 2021
 */

#include "cracker1.h"
#include "format.h"
#include "utils.h"
#include <string.h>
#include <unistd.h>

#include "./includes/queue.h"
#include <crypt.h>
#include <stdio.h>


typedef struct _job_t {
  char *user;
  char *password;
  char *password_h;
} job_t;

pthread_mutex_t thread1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thread2 = PTHREAD_MUTEX_INITIALIZER;
static int fails = 0;
static int recovers = 0;
static queue * que = NULL;

void *helper(void *input);

int start(size_t thread_count) {
    // TODO your code here, make sure to use thread_count!
    // Remember to ONLY crack passwords in other threads
    que = queue_create(-1);
    char *buf = NULL;
    size_t s = 0;
    ssize_t bytes_read;
    while (1) {
      bytes_read = getline(&buf,&s, stdin);
      if (bytes_read == -1) break;
      if (bytes_read>0 && buf[bytes_read-1] == '\n') buf[bytes_read-1] = '\0';
      job_t *task = calloc(1, sizeof(job_t));
      char *user = strtok(buf, " ");
      char *password_h = strtok(NULL, " ");
      char *password = strtok(NULL, " ");
      task->password = calloc(strlen(password)+1, sizeof(char));
      strcpy(task->password, password);
      task->password_h = calloc(strlen(password_h)+1, sizeof(char));
      strcpy(task->password_h, password_h);
      task->user = calloc(strlen(user)+1, sizeof(char));
      strcpy(task->user, user);
      queue_push(que, task);
    }
    queue_push(que, NULL);
    free(buf);

    pthread_t tid[thread_count];
    for (size_t i = 0; i < thread_count; i++) {
      pthread_create(tid+i, NULL, helper, (void *)(i+1));
      if (i == thread_count-1) {
        for (size_t j = 0; j < thread_count; j++) {
          pthread_join(tid[j], NULL);
        }
      }
    }
    queue_destroy(que);
    v1_print_summary(recovers, fails);
    pthread_mutex_destroy(&thread1);
    pthread_mutex_destroy(&thread2);
    return 0; 
    // DO NOT change the return code since AG uses it to check if your
    // program exited normally
}


void *helper(void *input) {
  int thread = (long) input;
  struct crypt_data cdata;
  cdata.initialized = 0;
  job_t *task = NULL;

  while ((task = queue_pull(que))) {
    v1_print_thread_start(thread, task->user);
    double start = getThreadCPUTime();
    int solve = 0;
    char *password = calloc(9, sizeof(char));
    int count = 0;
    strcpy(password, task->password);
    setStringPosition(password+getPrefixLength(password), 0);
    char *inputs = NULL;

    while (1) {
      inputs = crypt_r(password, "xx", &cdata);
      count++;
      if (!strcmp(inputs, task->password_h)) {
        double duration = getThreadCPUTime() - start;
        v1_print_thread_result(thread, task->user, password, count, duration, 0);
        solve = 1;
        pthread_mutex_lock(&thread1);
        recovers++;
        pthread_mutex_unlock(&thread1);
        break;
      }
      incrementString(password);
      if (strncmp(password, task->password, getPrefixLength(task->password)))
        break;
    }

    free(password);

    if (!solve) {
      double duration = getThreadCPUTime() - start;
      v1_print_thread_result(thread, task->user, NULL, count, duration, 1);
      pthread_mutex_lock(&thread2);
      fails++;
      pthread_mutex_unlock(&thread2);
    }
    
    free(task->password_h);
    free(task->password);
    free(task->user);
    free(task);
  }
  queue_push(que, NULL);
  return NULL;
}
