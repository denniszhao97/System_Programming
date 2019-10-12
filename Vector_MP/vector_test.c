/**
 * Vector Lab
 * CS 241 - Spring 2019
 */
 
#include "vector.h"
#include "callbacks.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    // Write your test cases here
    vector *myvector = vector_create(&double_copy_constructor, &double_destructor, &double_default_constructor); 
    //rintf("%d\n",Myvector->capacity);
    if(myvector == NULL){
      printf("fail");
    } else {
      printf("%lu\n",vector_size(myvector));
      printf("%p\n",myvector);
      printf("%lu\n", vector_capacity(myvector));
    }

    double i = 8.0;
    double j = 6.0;
    double z = 9.0;
    double a = 1.0;
    double b = 2.0;
    double c = 3.0;
 
    vector_push_back(myvector, &i);
    vector_push_back(myvector, &j);
    vector_push_back(myvector, &z);
    vector_push_back(myvector, &a);
    vector_push_back(myvector, &b);
    vector_push_back(myvector, &c);

    printf("%lu\n",vector_size(myvector));

    size_t p = 1;
    vector_set(myvector, p, &i);
    vector_pop_back(myvector);
    vector_pop_back(myvector);
    vector_insert(myvector, p, &c);
    vector_erase(myvector, p);

    vector_reserve(myvector,10);
    printf("%lu\n",vector_size(myvector));
    printf("%lu\n", vector_capacity(myvector));

    void **temp = vector_begin(myvector);
    for(;temp!=vector_end(myvector); temp++){
       double *result = *temp;  
       printf("%f,",*result);
    }
    printf("\n");
 
    vector_set(myvector, p, &j);
    double *get_double = vector_get(myvector, p);
    printf("%f\n",*get_double);

    double *front = *vector_front(myvector);
    printf("%f\n",*front);
     
    double *end = *vector_back(myvector);
    printf("%f\n",*end);
    
    vector_clear(myvector);
    if(vector_begin(myvector) == vector_end(myvector)){
      printf("success\n");
    }
       
    vector_destroy(myvector); 
    return 0;
}
