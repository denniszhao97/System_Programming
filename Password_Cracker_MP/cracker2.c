/**
 * Password Cracker Lab
 * CS 241 - Spring 2019
 */
#include "cracker2.h"
#include "format.h"
#include "utils.h"
#include <crypt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t mybarrier;
static char passwords[14];
static int hash_count;
static int exit_flag;
static int success_flag;
static int num_threads;
static double cpu_sum;

typedef struct task{
	char usernames[9];
	char password_hash[14];
	char knowpart[9];
	int prefix_length;
} task;

typedef struct thread_pointer{
	pthread_t thread;
	int threadID;
} thread_pointer;

static task *cur = NULL;

void * start_rountine(void * inputs){
	thread_pointer * temp = (thread_pointer*)inputs;
	char initial_guess[14];
	char answer[14];
  char *hashed_result;
	struct crypt_data cdata;
	cdata.initialized = 0;

	while(1){
		int thread_status = 2;
		pthread_barrier_wait(&mybarrier); // wait for all other threads
		if(exit_flag == -1){
                    break;
                  }
                long start_index, count;
		getSubrange(strlen(cur->knowpart) - cur->prefix_length, num_threads, temp->threadID,&start_index, &count);
		strcpy(initial_guess, cur->knowpart);
		setStringPosition(initial_guess+cur->prefix_length, start_index);
		v2_print_thread_start(temp->threadID, cur->usernames, start_index,initial_guess);
		double cpu_i = getThreadCPUTime();
                int num_hashes = 0;
		for(long i = 0; i < count; i++){
                    pthread_mutex_lock(&m);
                     int success = success_flag;
                     pthread_mutex_unlock(&m);
			if(success == 0){
				thread_status = 1;
				break;
			}
			num_hashes++;
			hashed_result = crypt_r(initial_guess, "xx", &cdata);
			strcpy(answer, hashed_result);
			if(!strcmp(cur->password_hash, answer)){
				strcpy(passwords, initial_guess);
                                pthread_mutex_lock(&m);
				success_flag = 0;
                                pthread_mutex_unlock(&m);
				thread_status = 0;
				break;
			}
			incrementString(initial_guess + cur->prefix_length);
		}
		double cpu_f = getThreadCPUTime();
        
		pthread_mutex_lock(&m);
		cpu_sum += (cpu_f - cpu_i);
                hash_count += num_hashes;
		pthread_mutex_unlock(&m);

    v2_print_thread_result(temp->threadID, num_hashes, thread_status);
		pthread_barrier_wait(&mybarrier);
	}
	return NULL;
}

int start(size_t thread_count) {
	cur = malloc(sizeof(task));
	pthread_barrier_init (&mybarrier, NULL, thread_count+1);
	pthread_mutex_init(&m, NULL);
	num_threads = thread_count;
	success_flag = 1;
	thread_pointer ** threads = malloc(sizeof(thread_pointer*)*thread_count);
	for(size_t i = 0; i < thread_count; i++){
		threads[i] = malloc(sizeof(thread_pointer));
		threads[i]->threadID = i+1;
    pthread_create(&(threads[i]->thread), NULL, start_rountine, (void*)threads[i]);
	}

  char usernames[9];
  char password_hash[14];
  char knownpart[9];
  char* temp = malloc(100);
  size_t capacity = 100;

	while(1){
		hash_count = 0;
		cpu_sum = 0;
		double initial_time = getTime();
		double init_c = getThreadCPUTime();
    int read = getline(&temp, &capacity, stdin);
    exit_flag = read;
    if(exit_flag != -1){
      sscanf(temp, "%s %s %s", usernames, password_hash, knownpart);
      success_flag = 1;
    } else {
      pthread_barrier_wait(&mybarrier);
      break;
    }
		v2_print_start_user(usernames);
		strcpy((cur->usernames), usernames);
		strcpy((cur->password_hash),password_hash);
		strcpy((cur->knowpart), knownpart);
		int prefix_l = (getPrefixLength(cur->knowpart));
		cur->prefix_length = prefix_l;
		pthread_barrier_wait(&mybarrier);//last wait to let all begin
		pthread_barrier_wait(&mybarrier);// first wait to wait all finish
		double final_time = getTime();
		double fin_c = getThreadCPUTime();
		cpu_sum += fin_c - init_c;
		v2_print_summary(usernames, passwords, hash_count, final_time-initial_time, cpu_sum, success_flag);
	}
	for(size_t i = 0; i < thread_count; i++){
		pthread_join(threads[i]->thread, NULL);
	}

	for(size_t i = 0; i < thread_count; i++){
		free(threads[i]);
	}
	free(threads);
        free(cur);
        free(temp);
	pthread_mutex_destroy(&m);
	pthread_barrier_destroy(&mybarrier);
	return 0;
}

