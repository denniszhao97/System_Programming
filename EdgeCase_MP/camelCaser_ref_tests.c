/**
 * Extreme Edge Cases Lab
 * CS 241 - Spring 2019
 */
 
#include "camelCaser.h"
#include "camelCaser_ref_utils.h"
#include <unistd.h>
#include <stdio.h>
int main() {
    // Enter the string you want to test with the reference here
    // char* a = "\1d11HJello 76wOrLd.   !@ Love             YOU# that's not\' \\aHello$ CAPITaLEVERY? True. g\t\noodjob my boy.  small\aguy. hello\nworld\" by";
    // This function prints the reference implementation output on the terminal
   // int i = 100;
    char *input = "11 hello 44world  . 11Hello.";
   // for(;i<256;i++){
    //  input[i-100] = i; 
   // }
   //  input[156] = 0;
     print_camelCaser(input);

    // Enter your expected output as you would in camelCaser_utils.c
    char *output[] = {"\001d11hjello76World","","","loveYou","that","sNot","","ahello","capitalevery","true","gOodjobMyBoy","small\aguy","helloWorld",NULL};
    // This function compares the expected output you provided against
    // the output from the reference implementation
    // The function returns 1 if the outputs match, and 0 if the outputs
    // do not match
    int j = check_output(input, output);
    printf("%d\n",j);
    return j;
}
