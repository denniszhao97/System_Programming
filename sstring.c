/**
 * vector Lab
 * CS 241 - Spring 2019
 */

#include "sstring.h"
#include "vector.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <string.h>

struct sstring {
    // Anything you want
    vector *vector;
};

sstring *cstr_to_sstring(const char *input) {
    // your code goes here
    assert(input);
    //setups
    sstring *result = malloc(sizeof(sstring));
    result->vector = char_vector_create();
    char* input_s = malloc((strlen(input) + 1)*sizeof(char));
    if(input_s == NULL){
       return NULL;
    }
    strcpy(input_s, input);
    char* start = input_s;
    while(*input_s){
      vector_push_back(result->vector, input_s);
      input_s++;
    }
    free(start);
    return result;
}

char *sstring_to_cstr(sstring *input) {
    // your code goes here
    assert(input);
    size_t size = vector_size(input->vector);
    char *result = malloc((size+1)*sizeof(char));
    result[size] = 0;
    for(size_t i = 0; i < size; i++){
      char *temp = vector_get(input->vector, i);
      result[i] = *temp;
    }

    return result;
}

int sstring_append(sstring *this, sstring *addition) {
    assert(this);
    assert(addition);
    int result = vector_size(this->vector) + vector_size(addition->vector);
    size_t size = vector_size(addition->vector);
    for(size_t i = 0; i < size; i++){
      vector_push_back(this->vector, vector_get(addition->vector, i));
    }
    return result;
}

vector *sstring_split(sstring *this, char delimiter) {
    // your code goes here
    assert(this);
    vector *result = string_vector_create();
    size_t size = vector_size(this->vector);
    int start = 0;
    int end = 0;
    for(size_t i = 0; i < size; i++){
      char *cur = vector_get(this->vector, i);
      if( *cur == delimiter){
        end = (int)i;
        char *element = sstring_slice(this, start, end);
        vector_push_back(result, element);
        start = end + 1;
      }
    }
    char *element = sstring_slice(this, start, size);
    vector_push_back(result, element);
    

    return result;
}

int sstring_substitute(sstring *this, size_t offset, char *target,
                       char *substitution) {
    // your code goes here
    assert(this);
    assert(target);
    assert(substitution);

    size_t size = vector_size(this->vector);
    int success = 0;
    size_t target_index = 0;
    for(size_t i = offset; i < size; i++){
      char *cur = vector_get(this->vector, i);
      if(*cur == *target){     
        char *temp = target;
        temp++;
        success = 1;
        size_t j = i + 1;
        while(*temp){
           if(j >= size){
             success = 0;
             break;
           }
           cur = vector_get(this->vector, j); 
          if(*cur != *temp){
             success = 0;
             break;
           }
           temp++;
           j++;
        }
      }
      if(success){
        target_index = i;
        int n = strlen(target);
        int i = 0;
        for( ;i<n; i++){
          vector_erase(this->vector, target_index);
        }
         n = strlen(substitution);
         for(i = 0; i<n; i++){
           vector_insert(this->vector, target_index, substitution);
           target_index++;
           substitution++;
         }
         return 0;
       }
    }
    
 
    return -1;
}

char *sstring_slice(sstring *this, int start, int end) {
    // your code goes here
    assert(this);
    char *result = malloc((end - start + 1)*sizeof(char));
    char *temp = result;
    for(int i = start; i < end; i++){
      size_t z = i;
      char *cur = vector_get(this->vector, z);
      *result = *cur;
      result++;
    }
    *result = 0;
    return temp;
}

void sstring_destroy(sstring *this) {
    // your code goes here
    assert(this);
    vector_destroy(this->vector);
    free(this);
}

