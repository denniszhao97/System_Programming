/**
* Nonstop Networking
* CS 241 - Spring 2019
*/

#include "common.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


ssize_t read_message_size(int socket) {
    ssize_t size;
    ssize_t read_bytes = read_all(socket, (void*)&size, sizeof(size_t));
    if (read_bytes == 0 || read_bytes == -1) {
	return read_bytes;
    }
    return (ssize_t)size;
}


ssize_t read_all(int socket, char *buffer, size_t count) {
    ssize_t offset = 0;
    ssize_t result = 0;
    size_t temp = count;
    while (1) {
    	result = read(socket, buffer + offset, count - offset);
    	if (result == -1) {
    	    if (errno == EINTR) {
    		    continue;
    	    } else {
    		    return -1;
    	    }
    	}

    	offset += result;
      temp -= result;
      if (result == 0 || temp <= 0) {
          break;
      }
    }
    return offset;
}

ssize_t write_all(int socket, const char *buffer, size_t count) {

    ssize_t offset = 0;
    ssize_t result = 0;
    size_t temp = count;
    while (1) {
        result = write(socket, (void*)(buffer + offset), count - offset);
        if (result == -1) {
            if (errno == EINTR){
              continue;
            } else {
              return -1;
            }
        }
        offset += result;
        temp -= result;
        if (result == 0 || temp <= 0) {
            break;
        }
    }
    return offset;
}
