/**
 * Password Cracker Lab
 * CS 241 - Spring 2019
 */
#include "cracker1.h"
#include "format.h"
#include "utils.h"
#include <crypt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include <string.h>
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int success_counter = 0;
static int fail_counter = 0;
static double total_time = 0;

typedef struct task{
        char usernames[9];
        char password_hash[14];
        char knownpart[9];
        int prefix_length;
} task;

typedef struct thread_pointer{
        pthread_t thread;
        int threadID;
        queue *myqueue;
} thread_pointer;

void *start_rountine(void *input){
        char * hashed_result;
        char answer[14];
        int success = 0;

        struct crypt_data cdata;
        cdata.initialized = 0;
        thread_pointer *cur = (thread_pointer*)input;
        task *temp = (task *)queue_pull(cur->myqueue);
        char initial_guess[14];
         char initial[14];
        while(temp){
          strcpy(initial_guess, temp->knownpart);
          setStringPosition(initial_guess + temp->prefix_length, 0);
           strcpy(initial, initial_guess);
                int hash_counter = 0;
                v1_print_thread_start(cur->threadID, temp->usernames);
                while(1){
                        hash_counter++;
                        success = 0;
                        hashed_result = crypt_r(initial, "xx", &cdata);
                        strcpy(answer, hashed_result);
                        if(!strcmp(temp->password_hash, answer)){
                          break;
                        }
                        if(!(incrementString(initial + temp->prefix_length))){
                          success = 1;
                          break;
                        }
                }

                if(!success){
                        pthread_mutex_lock(&m);
                        success_counter++;
                        pthread_mutex_unlock(&m);
                }
                else{
                        pthread_mutex_lock(&m);
                        fail_counter++;
                        pthread_mutex_unlock(&m);
                }

                v1_print_thread_result(cur->threadID, temp->usernames, initial, hash_counter, getThreadCPUTime(), success);
                pthread_mutex_lock(&m);
                total_time += getThreadCPUTime();
                pthread_mutex_unlock(&m);
                temp = (task *)queue_pull(cur->myqueue);
        }

        return NULL;
}

int start(size_t thread_count) {

        queue *this_queue = queue_create(-1);
        task **users = malloc(sizeof(task*)*100);
        thread_pointer **threads = malloc(sizeof(thread_pointer*)*thread_count);
        for(size_t i = 0; i < 100; i++){
           if(i > 0 && i <= thread_count){
             threads[i-1] = malloc(sizeof(thread_pointer));
             threads[i-1]->threadID = i;
             threads[i-1]->myqueue = this_queue;
           }
                users[i] = malloc(sizeof(task));
        }

        char* temp = malloc(100);
        int read = 0;
        size_t capacity = 100;

        char usernames[9];
        char password_hash[14];
        char knownpart[9];
        long index = 0;
        while((read = getline(&temp, &capacity, stdin)) != -1){
                temp[read-1] = 0;
                sscanf(temp, "%s %s %s\n", usernames, password_hash, knownpart);
                strcpy((users[index]->usernames), usernames);
                strcpy((users[index]->password_hash), password_hash);
                strcpy((users[index]->knownpart), knownpart);
                int prefix_l = getPrefixLength(users[index]->knownpart);
                users[index]->prefix_length = prefix_l;
                queue_push(this_queue, (void*)users[index]);
                index++;
        }
        free(temp);
        for(size_t i = 0; i < thread_count; i++){
             queue_push(this_queue, (void*)NULL);
             pthread_create(&(threads[i]->thread), NULL, start_rountine, (void*)threads[i]);
      }

        for(size_t i = 0; i < thread_count; i++){
                pthread_join(threads[i]->thread, NULL);
        }
        v1_print_summary(success_counter, fail_counter);
        for(int i = 0; i < 100; i++){
                free(users[i]);
        }
        free(users);
        for(int i = 0; i < (int)thread_count; i++){
                free(threads[i]);
        }
        free(threads);
        queue_destroy(this_queue);
        pthread_mutex_destroy(&m);
        return 0;
}
