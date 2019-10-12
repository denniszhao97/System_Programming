#include "common.h"
#include "format.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


//static volatile int serverSocket;

char **parse_args(int argc, char **argv);
verb check_args(char **args);

char* get_response_header(int socket) {
  size_t length = 1024;
  char* header = malloc(length);
  char fd[2];
  size_t size = 1;
  int i = 0;
  while(fd[0] != '\n' || i == 0){
      ssize_t sz = read_all(socket, fd, 1);
      if (sz == -1) {
        exit(1);
      }
      header[i] = fd[0];
      i++;
      size++;
      if(size >= length){
        length += 1024;
        header = realloc(header, length);
      }
  }
  header[size - 1] = 0;
  return header;
}

void evaluate_header(char* header, int socket) {
    if (strcmp(header, "OK\n") == 0) {
      free(header);
    	return;
    }
    else if (strcmp(header, "ERROR\n") == 0) {
    	char* err = get_response_header(socket);
    	print_error_message(err);
    	free(header);
    	exit(10);
    } else{
    	print_invalid_response();
    	free(header);
    	exit(11);
    }
}

void process_response(int socket, verb request, char **args) {
  char buffer[1024];
  if (request == DELETE || request == PUT) {
       print_success();
  } else {
    if(request == GET){
        int local_file = open(args[4], O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
        size_t size = read_message_size(socket);
      	size_t read_count;
        size_t read_bytes = 0;
      	for (read_count = 0;read_count < size; read_count += read_bytes) {
      	    read_bytes = read_all(socket, buffer, 1024);
      	    if (read_bytes == 0) {
          		print_too_little_data();
          		exit(1);
      	    }
      	    write_all(local_file, buffer, read_bytes);
      	}

      	if (read_count > size || read_all(socket, buffer, 1024) > 0) {
      	    print_received_too_much_data();
      	    exit(1);
      	}
      	close(local_file);
    } else {
      ssize_t size = read_message_size(socket);
      ssize_t read_count;
      size_t read_bytes = 0;
      for (read_count = 0; read_count < size; read_count += read_bytes) {
        size_t read_bytes = read_all(socket, buffer, 1024);
        if (read_bytes == 0) {
        	print_too_little_data();
        	exit(1);
        }
        write_all(1, buffer, read_bytes);
      }
      write(1, "\n", 1);
      if (read_count > size || read_all(socket, buffer, 1024) > 0) {
        print_received_too_much_data();
        exit(1);
      }
    }
  }

}


int connect_to_server(const char* host, const char* port) {
  int client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if(client_socket == -1){
    fprintf(stderr, "ERROR: failed to create client \n");
    exit(1);
  }
  struct addrinfo hints, *result;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, port, &hints, &result) || connect(client_socket, result->ai_addr, result->ai_addrlen) == -1) {
    exit(1);
  }

  freeaddrinfo(result);
  return client_socket;

}

int main(int argc, char **argv) {
    char** args = parse_args(argc, argv);
    verb request = check_args(args);
    char* remote = args[3];
    char* local = NULL;
    if (request != DELETE) {
       local = args[4];
    }
    int serverSocket = connect_to_server(args[0], args[1]);
    char buffer[1024];
    size_t remote_len = strlen(remote);
    size_t request_len = strlen(args[2]);
    char *str = NULL;
    int clean_flag = 0;
    if (request == LIST) {
        str = "LIST\n";
    } else {
        size_t len = request_len + remote_len + 3;
        str = malloc(len);
        str[len-1] = 0;
        clean_flag = 1;
        sprintf(str, "%s %s\n", args[2], remote);
    }

    if (write_all(serverSocket, str, strlen(str)) == -1) {
      print_connection_closed();
      exit(1);
    }

    if(request == PUT){
      struct stat this;
      if (stat(local, &this) == -1) {
        perror("stat");
        exit(1);
      }
      size_t writesz = (size_t)this.st_size;
      if ((int)write_all(serverSocket, (char *)&writesz, sizeof(size_t)) == -1) {
        	print_connection_closed();
        	exit(1);
      }

      int local_file = open(local, O_RDONLY, S_IRWXU | S_IRWXG | S_IRWXO);
      if (local_file == -1) {
        print_connection_closed();
        exit(1);
      }

      size_t count = read(local_file, buffer, 1024);
      size_t written = 0;
      for(size_t write_count = 0; write_count < writesz; write_count += written) {
        	if ((int)count == -1) {
        	  print_connection_closed();
        	  exit(1);
        	}
        	written = write_all(serverSocket, buffer, count);
        	count = read(local_file, buffer, 1024);
      }
      close(local_file);
    }

    if (shutdown(serverSocket, SHUT_WR)) {
      perror("shutdown()");
      exit(3);
    }

    char* header = get_response_header(serverSocket);
    evaluate_header(header,serverSocket);
    process_response(serverSocket, request, args);

    close(serverSocket);
    if(clean_flag){
      free(str);
    }
   free(args);

}

/**
 * Given commandline argc and argv, parses argv.
 *
 * argc argc from main()
 * argv argv from main()
 *
 * Returns char* array in form of {host, port, method, remote, local, NULL}
 * where `method` is ALL CAPS
 */
char **parse_args(int argc, char **argv) {
    if (argc < 3) {
        return NULL;
    }

    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (port == NULL) {
        return NULL;
    }

    char **args = calloc(1, 6 * sizeof(char *));
    args[0] = host;
    args[1] = port;
    args[2] = argv[2];
    char *temp = args[2];
    while (*temp) {
        *temp = toupper((unsigned char)*temp);
        temp++;
    }
    if (argc > 3) {
        args[3] = argv[3];
    }
    if (argc > 4) {
        args[4] = argv[4];
    }

    return args;
}

/**
 * Validates args to program.  If `args` are not valid, help information for the
 * program is printed.
 *
 * args     arguments to parse
 *
 * Returns a verb which corresponds to the request method
 */
verb check_args(char **args) {
    if (args == NULL) {
        print_client_usage();
        exit(1);
    }

    char *command = args[2];

    if (strcmp(command, "LIST") == 0) {
        return LIST;
    }

    if (strcmp(command, "GET") == 0) {
        if (args[3] != NULL && args[4] != NULL) {
            return GET;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "DELETE") == 0) {
        if (args[3] != NULL) {
            return DELETE;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "PUT") == 0) {
        if (args[3] == NULL || args[4] == NULL) {
            print_client_help();
            exit(1);
        }
        return PUT;
    }

    // Not a valid Method
    print_client_help();
    exit(1);
}
