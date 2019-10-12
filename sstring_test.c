/**
 * Vector Lab
 * CS 241 - Spring 2019
 */
 
#include "sstring.h"
#include "callbacks.h"
#include "vector.h"
#include <stdio.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <string.h>
int main(int argc, char *argv[]) {
    // TODO create some tests
    char *input = "This is a";
    char *input2 = " {} day, {}!";
    sstring *t = cstr_to_sstring(input);
    sstring *t2 = cstr_to_sstring(input2);
    int i = sstring_append(t,t2);
    printf("%d\n",i);
        
    char *result = sstring_slice(t, 1, 6);
    char *r = result;
    while(*result){
      printf("%c",*result);
      result++;
    }
     printf("\n");
 
   
   char* target = "{}";
    char* substitution = "friend";
    char* sub2 = "good";
    int z = sstring_substitute(t, 18, target, substitution);
    int z2 = sstring_substitute(t, 0, target, sub2);
    int z3 = sstring_substitute(t, 0, target, sub2);
    printf("%d, %d, %d\n",z, z2, z3);
     
    char *t_back = sstring_to_cstr(t);
    char *clea = t_back;
    while(*t_back){
      printf("%c",*t_back);
      t_back++;
    }
    printf("\n");


    char delimiter = ' ';
    size_t j = 0;
    vector *myvector = sstring_split(t,delimiter);
    for(; j < vector_size(myvector); j++){
    char *cur = vector_get(myvector,j);
    char **a = &cur;
   printf("%s,",*a);
  }

    printf("\n %lu\n", vector_size(myvector));
   
    sstring_destroy(t);
    sstring_destroy(t2);
    free(r);
    free(clea);
    vector_destroy(myvector);
    return 0;
}
