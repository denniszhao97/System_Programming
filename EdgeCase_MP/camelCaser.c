 #include "camelCaser.h"
 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>
 #include <stdio.h>

 int numberOfPointers(char *input){
     char* temp = input;
     int punct = 0;
     while(*temp){
         if(ispunct(*temp)){
             punct++;
         }
         temp++;
     }
     return punct;
 }

 int isItSpecial(char* input){
     if(isalpha(*input) || ispunct(*input) || isspace(*input)){
       return 1;
     }
     return 0;
 }

 char **camel_caser(const char *input_str) {
     if(input_str == NULL){
       return NULL;
     }
     //setups
     char* input_s = malloc(strlen(input_str) + 1);
     if(input_s == NULL){
        return NULL;
     }
     strcpy(input_s, input_str);
     char* start = input_s;
     int numberOfPointer = numberOfPointers(input_s);
     char** output_s= malloc((numberOfPointer+1) * sizeof(char*));
     if(output_s == NULL){
       return NULL;
     }
     output_s[numberOfPointer] = NULL;
     if(numberOfPointer == 0){
       free(start);
       return output_s;
     }
     char* temp = input_s;
     int i = 0;
     int characters = 1;
     while(i < numberOfPointer){
       if(isspace(*temp)){
         temp++;
         continue;
       }

       if(!ispunct(*temp)){
         characters++;
       } else{
          output_s[i] = malloc(characters*sizeof(char));
          if(output_s[i] == NULL){
             return NULL;
          }
          output_s[i][characters-1] = '\0';
          characters = 1;
          i++;
       }
       temp++;
     }


     //function starts
     int newSentence = 1;
     int newWord = 1;
     i = 0;
     int j = 0;
     while(i < numberOfPointer){
       if(!isItSpecial(input_s)){
         output_s[i][j] = *input_s;
         j++;
         input_s++;
         if(isspace(*input_s)){
           newSentence = 0;
         }
         continue;
       } else{
         if(isspace(*input_s)){
           newWord = 1;
           input_s++;
           continue;
         }
         if(isalpha(*input_s)){
           if(newWord == 1){
             if(newSentence == 1){
               output_s[i][j] = tolower(*input_s);
               newWord = 0;
               newSentence = 0;
               j++;
             } else{
               output_s[i][j] = toupper(*input_s);
               newWord = 0;
               j++;
             }
         } else {
           output_s[i][j] = tolower(*input_s);
           j++;
         }
       } else {
         newSentence = 1;
         newWord = 1;
         i++;
         j = 0;
       }
     }
       input_s++;
   }
    free(start);
    return output_s;
 }

 void destroy(char **result) {
   if(result == NULL){
     return;
   }
   char** t = result;
   while(*result){
    free(*result);
    *result = NULL;
    result++;
  }
    free(t);
    t = NULL;
    return;
}
