/**
 * Parallel Make Lab
 * CS 241 - Spring 2019
 */


#include "format.h"
#include "graph.h"
#include "queue.h"
#include "parmake.h"
#include "parser.h"
#include <pthread.h>
#include "set.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>

static set* is_visited; //unvisited
static set* cycle_nodes; //nodes cause cycle
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;

typedef struct _data {
	queue* tasks;
	set* tasks_element;
	set* all_tasks;
  graph* dependency_graph;
} data;

int run(graph* dependency_graph, char* cur);
int system_command(vector* cmds);
int is_file(char* pathname);

int is_file(char* pathname) {
    int success = access(pathname, F_OK);
    if(success == 0){
      return 1;
    }
    return 0;
}

void *start_routine(void* inputs){
  data *temp = (data*)inputs;
  graph* dependency_graph = temp->dependency_graph;
  while (1) {
		char* cur = queue_pull(temp->tasks);
		if (!strcmp(cur, "")) {
			queue_push(temp->tasks, "");
			return NULL;
		}
		rule_t* cur_rule = (rule_t *)graph_get_vertex_value(dependency_graph, cur);
    int isfile = is_file(cur);
		if (!set_contains(temp->all_tasks, cur)) { // do not need to process
			continue;
		}

      vector* dependencies = graph_neighbors(dependency_graph, cur);
    		if (!vector_size(dependencies)) {
					int now = system_command(cur_rule->commands);
					pthread_mutex_lock(&m);
    			cur_rule->state = now;
					pthread_mutex_unlock(&m);
    		} else {
    			int success = 1;
    			for (size_t i = 0; i < vector_size(dependencies); i++) {
  					char* this = vector_get(dependencies, i);
            rule_t* this_rule = (rule_t *)graph_get_vertex_value(dependency_graph, this);
            if(!is_file(this)){
              if (this_rule->state == -1) {
                success = 0;
								pthread_mutex_lock(&m);
                cur_rule->state = -1;
								pthread_mutex_unlock(&m);
                break;
              } else if(this_rule->state == 1){
                continue;
              }
            } else {
              if(isfile){
                struct stat child;
                stat(this, &child);
                struct stat parent;
                stat(cur, &parent);
                if (difftime(parent.st_mtime, child.st_mtime) > 0){
									      pthread_mutex_lock(&m);
                        cur_rule->state = 1;
												pthread_mutex_unlock(&m);
                        success = 0;
                        break;
      						} else {
                    if(this_rule->state == -1) {
                      success = 0;
											pthread_mutex_lock(&m);
                      cur_rule->state = -1;
											pthread_mutex_unlock(&m);
                      break;
                  }
                }
      				} else{
                if (this_rule->state == -1) {
                  success = 0;
									pthread_mutex_lock(&m);
                  cur_rule->state = -1;
									pthread_mutex_unlock(&m);
                  break;
                } else if(this_rule->state == 1){
                  continue;
                }
              }
             }
            }
      			if (success) {
							int now = system_command(cur_rule->commands);
							pthread_mutex_lock(&m);
							cur_rule->state = now;
							pthread_mutex_unlock(&m);
      			}
    		}
        vector_destroy(dependencies);


			vector* antidependencies = graph_antineighbors(dependency_graph, cur);

			for (size_t i = 0; i < vector_size(antidependencies); i++) {
				char* parent_node = vector_get(antidependencies , i);
				vector* this_denpendencies = graph_neighbors(dependency_graph,  parent_node);
				int good = 1;
				for (size_t j = 0; j < vector_size(this_denpendencies); j++) {
					char* child_node = vector_get(this_denpendencies, j);
					rule_t* child_rule = (rule_t *)graph_get_vertex_value(dependency_graph, child_node);
					pthread_mutex_lock(&m);
					int child_state = child_rule->state;
					pthread_mutex_unlock(&m);
					if(child_state == 0){
						good = 0;
						break;
					}
				}
				if(good){
					pthread_mutex_lock(&m);
					if (!set_contains(temp->tasks_element, parent_node)){
						set_add(temp->tasks_element, parent_node);
						pthread_mutex_unlock(&m);
						queue_push(temp->tasks, parent_node);
					} else{
						pthread_mutex_unlock(&m);
					}
				}
				vector_destroy(this_denpendencies);
			}
			vector_destroy(antidependencies);
	}
  return NULL;
}

int isCyclic(graph* dependency_graph, char* cur) {
    set_add(is_visited, cur);
    int success = 0;
    vector* dependencies = graph_neighbors(dependency_graph, cur);
    for(size_t i = 0; i < vector_size(dependencies); i++){
       char* temp = vector_get(dependencies, i);
       if(set_contains(is_visited, temp)){
         set_add(cycle_nodes, cur);
         success = 1;
         continue;
       }
       if(isCyclic(dependency_graph, temp) && strcmp(cur, "")){
         set_add(cycle_nodes, cur);
         success = 1;
       }
       set_remove(is_visited,temp);
    }
    vector_destroy(dependencies);
    return success;
}


int system_command(vector* commands) {
    for (size_t i = 0; i < vector_size(commands); i++) {
            char* command = vector_get(commands, i);
            if (system(command)) {
                return -1;
            }
    }
    return 1;
}

int run(graph* dependency_graph, char* cur) {
    rule_t* rule = (rule_t *)graph_get_vertex_value(dependency_graph, cur);
    int isfile = is_file(cur);
    int state = 1;
    int success = 1;
    vector* dependencies = graph_neighbors(dependency_graph, cur);
    if (!vector_size(dependencies)) { // no dependencies
            state = system_command(rule->commands);
            rule->state = state;
    } else {
      for (size_t i = 0; i < vector_size(dependencies); i++) {
          char* temp = vector_get(dependencies, i);
          if (!is_file(temp)) {
            rule_t* rule_v = (rule_t *)graph_get_vertex_value(dependency_graph, temp);
            if (rule_v->state > 0){
              continue;
            }
            if (run(dependency_graph, temp) < 0) {
               state = -1;
            }
          } else {
           if(isfile){
             struct stat child;
             stat(temp, &child);
             struct stat parent;
             stat(cur, &parent);
             if (difftime(parent.st_mtime, child.st_mtime) > 0){
                     rule->state = 1;
                     success = 0;
                     continue;
             } else {
               if (run(dependency_graph, temp) < 0){
                state = -1;
               }
             }
           } else {
             if (run(dependency_graph, temp) < 0) {
                     success = 0;
                     rule->state = -1;
                     break;
             }
           }
         }
      }
      if (state > 0 && success){
        state = system_command(rule->commands);
        rule->state = state;
      }
    }
    vector_destroy(dependencies);
    return state;
}

void initialize_queue(graph* dependency_graph, queue* tasks, char* temp, set* all_tasks, set* tasks_element) {
	set_add(all_tasks, temp);
	vector* dependencies = graph_neighbors(dependency_graph, temp);
	if (!vector_size(dependencies) && !set_contains(tasks_element, temp)) {
		queue_push(tasks, temp);
		set_add(tasks_element, temp);
	}
	for (size_t i = 0; i < vector_size(dependencies); i++) {
		initialize_queue(dependency_graph, tasks, vector_get(dependencies, i), all_tasks, tasks_element);
	}
	vector_destroy(dependencies);
}

int parmake(char *makefile, size_t num_threads, char **targets) {
    // good luck!

    graph *dependency_graph = parser_parse_makefile(makefile, targets);
    vector *nodes = graph_vertices(dependency_graph);
    char *first_rule_temp = vector_get(nodes, 1);
    char *first_rule = malloc(strlen(first_rule_temp)+1);
    first_rule[strlen(first_rule_temp)] = 0;
    strcpy(first_rule,first_rule_temp);
    is_visited = string_set_create();
    cycle_nodes = string_set_create();
    isCyclic(dependency_graph, vector_get(nodes, 0));
    vector *cycle_ele = set_elements(cycle_nodes);
    for(size_t i = 0; i < vector_size(cycle_ele); i++){
      char *temp = vector_get(cycle_ele, i);
       graph_remove_vertex(dependency_graph, temp);
     }
    if(num_threads == 1){
      if(targets[0] == NULL){
        targets[0] = first_rule;
        targets[1] = NULL;
      }
      char** temp = targets;
      while(*temp){
        if(set_contains(cycle_nodes, *temp)){
          print_cycle_failure(*temp);
        } else {
          run(dependency_graph, *temp);
        }
        temp++;
      }
    }

    if(num_threads > 1){
      if(targets[0] == NULL){
        targets[0] = first_rule;
        targets[1] = NULL;
      }
    	queue* tasks = queue_create(-1);
    	set* tasks_element = string_set_create();
    	set* all_tasks = string_set_create();
    	data *work = malloc(sizeof(data));
    	work->tasks = tasks;
    	work->tasks_element = tasks_element;
    	work->all_tasks = all_tasks;
      work->dependency_graph = dependency_graph;
    	pthread_t *threads = malloc(num_threads*sizeof(pthread_t));
      char** temp = targets;
      while(*temp){
        if(set_contains(cycle_nodes, *temp)){
          print_cycle_failure(*temp);
        } else {
          initialize_queue(dependency_graph, tasks, *temp, all_tasks, tasks_element);
        }
        temp++;
      }
      if(!set_empty(tasks_element)){
        for (size_t i = 0; i < num_threads; i++) {
      		pthread_create(threads+i,NULL, start_routine, (void*)work);
      	}
      	for (size_t i = 0; i < num_threads; i++) {
      		pthread_join(threads[i], NULL);
      	}
      }

    	queue_destroy(tasks);
    	set_destroy(all_tasks);
    	set_destroy(tasks_element);
      free(threads);
      free(work);
    }

    free(first_rule);
    vector_destroy(cycle_ele);
    vector_destroy(nodes);
    set_destroy(is_visited);
    set_destroy(cycle_nodes);
    graph_destroy(dependency_graph);
    pthread_mutex_destroy(&m);
		pthread_mutex_destroy(&m1);
    return 0;
}

