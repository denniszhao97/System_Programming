/**
 * Extreme Edge Cases Lab
 * CS 241 - Spring 2019
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camelCaser.h"
#include "camelCaser_tests.h"

/*
 * Testing function for various implementations of camelCaser.
 *
 * @param  camelCaser   A pointer to the target camelCaser function.
 * @param  destroy      A pointer to the function that destroys camelCaser
 * output.
 * @return              Correctness of the program (0 for wrong, 1 for correct).
 */
int test_camelCaser(char **(*camelCaser)(const char *),
                    void (*destroy)(char **)) {
    // TODO: Return 1 if the passed in function works properly
    char* t1 = NULL;
    char** a1 = (*camelCaser)(t1);
    if( a1 != NULL){
       return 0;
    }

    
    char *t2 = "\\a dk zzh. \1d11HJello 76wOrLd.   !@ Love             YOU# that's not\' \\aHello$ CAPITaLEVERY? True. g\t\noodjob my boy.  small\aguy. 11 hello world. 11Hello. hello\nworld\"   by";
    char *answer2[] = {"","aDkZzh","\001d11hjello76World","","","loveYou","that","sNot","","ahello","capitalevery","true","gOodjobMyBoy","small\aguy","11HelloWorld","11hello","helloWorld",NULL};
    char** a2 = (*camelCaser)(t2);
    int pointerNumber = sizeof(answer2) / sizeof(char*);
    char** temp = a2;
    int count = 0;
    while(*temp){
     count++;
     temp++;
    }
    if(count+1!=pointerNumber){
       (*destroy)(a2);
       return 0;
    }

    char** start = a2;
    int i = 0;
    while(*a2){
      if(strlen(*a2) != strlen(answer2[i])){
         (*destroy)(start);
         return 0;
      }
      if(strcmp(*a2,answer2[i])){
         (*destroy)(start);
          return 0;
      }
      a2++;
      i++;
      if((*a2 == NULL && answer2[i] != NULL) || (*a2 != NULL && answer2[i] == NULL)){
          (*destroy)(start);
          return 0;
      }
    }
   (*destroy)(start);
 
    char* t3 = "   ";
    char** a3 = (*camelCaser)(t3);
    if(a3 == NULL){
      (*destroy)(a3);
      return 0;
    } else{
      if(*a3 != NULL){
        (*destroy)(a3);
        return 0;
      }
    }
    (*destroy)(a3);

    char* t4 = "";
    char** a4 = (*camelCaser)(t4);
    if(a4 == NULL){
      (*destroy)(a4);
      return 0;
    } else{
      if(*a4 != NULL){
       (*destroy)(a4);
        return 0;
      }
    }
   (*destroy)(a4);

   return 1;
}
