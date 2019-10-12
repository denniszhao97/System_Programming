/**
 * Nonstop Networking
 * CS 241 - Spring 2019
 */
#include "common.h"
#include "format.h"
#include <vector.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <sys/epoll.h>

#define MAX_CLIENTS 128
#define MAX_EVENTS  MAX_CLIENTS + 1

typedef enum{
    READING_HEADER,
    READING_FROM_CLIENT,
    WRITING_ON_SERVER,
    WRITING_OK,
    WRITING_ERR,
    WRITING_ERRMSG,
    LIST_WRITEBACK,
    GET_WRITEBACK
} states;

typedef struct client_info {
    states      state;
    int         fd;
    verb        request;
    size_t      file_size;
    int         offset;
    char       *header;
    char       *filename;
    const char *err_msg;
    FILE       *client_file;
    char       *write_back_buffer;
} client_info;

static vector         *clients;
static vector         *sockets;
static vector      *all_files;
static char        *directory;
static int          epollfd;
static int          end;
static int         read_done;
static int         header_flag;
static int         max_filesize;

ssize_t get_header(client_info* cur_client, char *buffer, size_t max){
  //get the whole header in buffer
  ssize_t total = 0;
  ssize_t nread = 0;
  int socket = cur_client->fd;
  for( ; (size_t)total < max; total += nread) {
    nread = read(socket, (void*)buffer, 1);
    if(nread == 0){
      return -1;
    }
    if (nread == -1) {
      if(errno == EAGAIN || errno == EWOULDBLOCK){
        break;
      }
      if(errno != EINTR){
         return -1;
      }
      if(errno ==  EINTR){
         continue;
      }
    }

     if(buffer[0] == '\n'){ // end reached
       buffer[0] = 0;
       buffer += nread;
       header_flag = 1;
       break;
     }
     buffer += nread;
  }

  if((size_t)total >= max){
    return -1;
  }

  return total;
}

ssize_t server_read(client_info* cur_client, char* buffer, size_t count) {
  //server read from the client, we handle eagain and eouldblock same way.
    ssize_t total = 0;
    ssize_t nread = 0;
    int socket = cur_client->fd;
    for (; count > 0; count -= nread) {
         nread = read(socket, (void*)buffer, count);
        if (nread == -1) {
          if(errno == EAGAIN || errno == EWOULDBLOCK){
            break;
          }
          if(errno != EINTR){
             return -1;
          }
          if(errno ==  EINTR){
             continue;
          }
        }

        if (nread == 0) {
            read_done = 1;
            break;
        }

        total += nread;
        if (count > 0) {
          buffer += nread;
        }
    }
    return total;
}

ssize_t server_write(client_info* cur_client, const char* buffer, size_t count) {
  //server write back to the client, we handle eagain and eouldblock same way.
  ssize_t total = 0;
  ssize_t nwrite = 0;
  int socket = cur_client->fd;
  for (; count > 0; count -= nwrite) {
      nwrite = write(socket, (void*)buffer, count);
      if (nwrite == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK){
          break;
        }
        if(errno != EINTR){
           return -1;
        }
        if(errno ==  EINTR){
           continue;
        }
      }

      if (nwrite == 0) {
          break;
      }

      total += nwrite;
      if (count > 0) {
        buffer += nwrite;
      }
  }
  return total;
}

void new_clients(int fd, client_info* myclient){
   // initialize a client_info
    myclient->fd = fd;
    myclient->request = V_UNKNOWN;
    myclient->header = calloc(1,1025);
    myclient->write_back_buffer = NULL;
    myclient->filename = NULL;
    myclient->client_file = NULL;
    myclient->state = READING_HEADER;
    myclient->file_size = 0;
    myclient->offset = 0;
    vector_push_back(clients, myclient);
    vector_push_back(sockets, &fd);
}

int delete_file(char *filename){
  //delete a file, + 2 because one / one null byte
    size_t length = strlen(directory) + strlen(filename) + 2;
    char temp[length];
    sprintf(temp, "%s/%s", directory, filename);
    temp[length] = 0;
    return unlink(temp);
}

FILE *open_file(char *filename, char *operation){
  //open a file, + 2 because one / one null byte
  size_t length = strlen(directory) + strlen(filename) + 2;
  char temp[length];
  sprintf(temp, "%s/%s", directory, filename);
  temp[length] = 0;
  return fopen(temp, operation);
}


void do_error(client_info *cur_client, const char *err_msg){
  //ready to send error\n back to the client and set epollout because we need to write later
    cur_client->state   = WRITING_ERR;
    cur_client->offset = 0;
    cur_client->err_msg = err_msg;
    struct epoll_event ev;
    ev.events  = EPOLLOUT;
    ev.data.fd = cur_client->fd;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, cur_client->fd, &ev);
}

void do_ok(client_info *cur_client){
  //ready to send ok\n back to the client and set epollout because we need to write later
    cur_client->state = WRITING_OK;
    cur_client->offset = 0;
    struct epoll_event ev;
    ev.events  = EPOLLOUT;
    ev.data.fd = cur_client->fd;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, cur_client->fd, &ev);
}

void client_finished(client_info *cur_client){
  //this client's task is done, wait for the next available.
    epoll_ctl(epollfd, EPOLL_CTL_DEL, cur_client->fd, NULL);
    shutdown(cur_client->fd, SHUT_RDWR);
    close(cur_client->fd);
    for(size_t i= 0; i < vector_size(sockets); i++){
      int temp = *((int*)vector_get(sockets, i));
      if(temp == cur_client->fd){
        vector_erase(clients, i);
        vector_erase(sockets, i);
        break;
      }
    }
}

void write_response_ok(client_info *cur_client){
  //write ok\n back to server, if it is put or delete, then we are done.
    char *buffer  = "OK\n";
    int   expected  = 3 - cur_client->offset;
    int   nwrite   = server_write(cur_client, buffer + cur_client->offset, expected);
    if (nwrite == -1) {
        client_finished(cur_client);
    } else if (nwrite == expected) {
        if (cur_client->request == LIST){
          cur_client->state = LIST_WRITEBACK;
          cur_client->offset = 0;
        }

        if(cur_client->request == GET) {
          cur_client->state = GET_WRITEBACK;
          cur_client->offset = 0;
        }
        if (cur_client->request == PUT || cur_client->request == DELETE) {
            client_finished(cur_client);
        }
    } else {//interrupted
      cur_client->offset += nwrite;
    }
}


void write_response_error(client_info *cur_client){
   //write the error\n back
    char *buffer  = "ERROR\n";
    int   expect     = strlen(buffer);
    int   nwrite   = server_write(cur_client, buffer + cur_client->offset, expect);

    if (nwrite == -1) {
        client_finished(cur_client);
    }else if (nwrite == expect) {
        cur_client->state = WRITING_ERRMSG;
        cur_client->offset = 0;
    } else{ //interrupted
      cur_client->offset += nwrite;
    }
}

void write_response_errormsg(client_info *cur_client){
  // we have done writing error\n back, now tell client how it does wrong
    const char *buffer  = cur_client->err_msg + cur_client->offset;
    int   expect     = strlen(buffer);
    int   nwrite   = server_write(cur_client, buffer, expect);

    if (nwrite == -1) {
        client_finished(cur_client);
    }else if (nwrite == expect) {
        client_finished(cur_client);
    } else{ //interrupted
      cur_client->offset += nwrite;
    }
}

void list_writeback(client_info *cur_client){
  //we have composed the list result, now give client what it wants, we will determine the file size first
  char *buffer  = (char*)(&(cur_client->file_size)) + cur_client->offset;
  int   expect   = max_filesize - cur_client->offset;
  int   nwrite   = server_write(cur_client, buffer, expect);
  if (nwrite == -1) {
      client_finished(cur_client);
      return;
  } else if (nwrite == expect) {
      cur_client->offset = 0;
  } else{
      cur_client->offset += nwrite;
  }

    buffer  = cur_client->write_back_buffer + cur_client->offset;
    expect     = strlen(buffer);
    nwrite   = server_write(cur_client, buffer, expect);

    if (nwrite == -1 || nwrite == expect) {
        client_finished(cur_client);
    } else {//interrupted
      cur_client->offset += nwrite;
    }
}

void get_writeback(client_info *cur_client){
  //we have find the requiested file, now give client what it wants, we will determine the file size first
  char *buffer_1  = (char*)(&(cur_client->file_size)) + cur_client->offset;
  int   expect   = max_filesize - cur_client->offset;
  int   write_1   = server_write(cur_client, buffer_1, expect);

  if (write_1 == -1) {
      client_finished(cur_client);
      return;
  } else if (write_1 == expect) {
      cur_client->offset = 0;
  } else{
      cur_client->offset += write_1;
  }

  char buffer[1024];
  size_t written = 0;
  for(written = ftell(cur_client->client_file); written < cur_client->file_size; written = ftell(cur_client->client_file)) {
      int rem_size  = cur_client->file_size - written;
      int cur_write = 1024;
      if(rem_size <= 1024){
        cur_write = rem_size;
      }
      fread(buffer, 1, cur_write, cur_client->client_file);
      ssize_t nwrite = server_write(cur_client, buffer, cur_write);
      if (nwrite == -1) {
          fclose(cur_client->client_file);
          client_finished(cur_client);
          return;
      }
      if ((int)nwrite < cur_write) {
          fseek(cur_client->client_file, (int)nwrite - cur_write, SEEK_CUR);
          break;
      }
  }

  if (written == cur_client->file_size) {
      fclose(cur_client->client_file);
      client_finished(cur_client);
  }
}

void do_list(client_info *cur_client){
  // buffer is the largest possible list result, list_result is the final output
    int len = 257 * vector_size(all_files);
    if(len == 0){
      do_ok(cur_client);
      return;
    }
    char *buffer = calloc(1,len);
    char *start = buffer;
    size_t  size  = vector_size(all_files);
    int final_length = 0; // real result length counter
    for(size_t i = 0; i < size; i++){
        char* cur = (char*)(vector_get(all_files,i));
        memcpy(buffer, cur, strlen(cur));
        buffer += strlen(cur);
        final_length += strlen(cur) + 1;
        if (i != size - 1) { // in between, need new line character to seperate
            buffer[0] = '\n';
            buffer++;
        } else { //last one, null byte end
            buffer[0] = 0;
        }
    }
    char *list_result = malloc(final_length);
    list_result[final_length-1] = 0;
    memcpy(list_result, start, final_length - 1);
    cur_client->write_back_buffer = list_result;
    cur_client->file_size = final_length - 1;
    free(start);
    do_ok(cur_client);
}

void do_delete(client_info *cur_client){
  //find the required size and delete it.
  int  find_file = 0;
  for(size_t i = 0; i < vector_size(all_files); i++){
      char* cur = (char*)(vector_get(all_files,i));
      if (strcmp(cur, cur_client->filename) == 0) {
          find_file = 1;
          delete_file(cur_client->filename);
          vector_erase(all_files, i);
          break;
      }
  }

    if (!find_file) {
        do_error(cur_client, err_no_such_file);
    } else {
        do_ok(cur_client);
    }
}

void do_get(client_info *cur_client){
    // prepare the correct filesize and filename before we write back on clients.
    int file_exist = 0;
    //find whether the requested file exist
    for(size_t i = 0; i < vector_size(all_files); i++){
        char* cur = (char*)(vector_get(all_files,i));
        if(strcmp(cur, cur_client->filename) == 0){
          file_exist = 1;
        }
    }

    if (!file_exist) {
        do_error(cur_client, err_no_such_file);
    } else {
        cur_client->client_file = open_file(cur_client->filename, "r");
        if (!(cur_client->client_file)) {
            do_error(cur_client, err_no_such_file);
            return;
        }
        fseek(cur_client->client_file, 0, SEEK_END);
        cur_client->file_size = ftell(cur_client->client_file);
        fseek(cur_client->client_file, 0, SEEK_SET);
        do_ok(cur_client);
    }
}

void do_put(client_info *cur_client){
    // prepare the correct filesize before we put on server.
    char *buffer  = (char*)(&(cur_client->file_size)) + cur_client->offset;
    int   expect     = max_filesize - cur_client->offset;
    int   nread   = server_read(cur_client, buffer, expect);
    if (nread == -1) {
        do_error(cur_client, err_bad_file_size);
        return;
    }else if (nread == expect) {
        cur_client->state = WRITING_ON_SERVER;
        cur_client->client_file = open_file(cur_client->filename, "w");
        if (!(cur_client->client_file)) {
            do_error(cur_client, err_no_such_file);
            return;
        }
    } else {
        cur_client->offset += nread;
    }
}

void write_at_server(client_info *cur_client){
  char buffer[1024];
  size_t nread = 0;
  //read 1024 and write 1024 every time, if at the middle we find it is a bad file size, we delete the file and return
  for (nread = ftell(cur_client->client_file); nread < cur_client->file_size; nread = ftell(cur_client->client_file)) {
      memset(buffer, 0, 1024);
      int rem_size  = cur_client->file_size  - nread;
      int cur_read = 1024;
      if(rem_size <= 1024){
        cur_read = rem_size;
      }
      read_done = 0;
      int read_temp = server_read(cur_client, buffer, cur_read);
      if (read_temp == -1 || read_done == 1) {
          fclose(cur_client->client_file);
          delete_file(cur_client->filename);
          do_error(cur_client,  err_bad_file_size);
          //if read_done == 1 then it means, it reads less than expect at that time
          if(read_done == 1){
            print_too_little_data();
          }
          return;
      }
      fwrite(buffer, 1, read_temp, cur_client->client_file);
  }
  //what we read is bigger than the expected size.
  if (nread > cur_client->file_size) {
     fclose(cur_client->client_file);
     delete_file(cur_client->filename);
     do_error(cur_client, err_bad_file_size);
     print_received_too_much_data();
  } else {
    fclose(cur_client->client_file);
    //should not be able to read more data
    int temp = server_read(cur_client, buffer, 100);
    if (temp) {
        delete_file(cur_client->filename);
        do_error(cur_client, err_bad_file_size);
        print_received_too_much_data();
        return;
    }
    vector_push_back(all_files, cur_client->filename);
    do_ok(cur_client);
  }
}

int parse_header(client_info *cur_client){
  // parse the request
  char* verb_now = calloc(1,16);
  cur_client->filename = calloc(1,strlen(cur_client->header));
  int check = sscanf(cur_client->header, "%s %s", verb_now, cur_client->filename);
  if(strlen(cur_client->filename) >= 255){
    return -1;
  }
  if (check == 0) {
      return -1;
  }
  if(check != 2){
    if(check == 1){
      if(strcmp(verb_now, "LIST")){
        free(verb_now);
        return -1;
      }
    } else {
      free(verb_now);
      return -1;
    }
  }

  if (strcmp(verb_now, "LIST") == 0) {
      cur_client->request = LIST;
  } else if (strcmp(verb_now, "GET") == 0) {
      cur_client->request = GET;
  } else if (strcmp(verb_now, "DELETE") == 0) {
      cur_client->request = DELETE;
  } else if (strcmp(verb_now, "PUT") == 0) {
      cur_client->request = PUT;
      cur_client->state = READING_FROM_CLIENT;
      cur_client->offset = 0;
      //if the put file is already exist, remove it and prepare to overwrite
      for(size_t i = 0; i < vector_size(all_files); i++){
        char *cur = (char*)vector_get(all_files, i);
        if(strcmp(cur, cur_client->filename) == 0){
          delete_file(cur_client->filename);
          vector_erase(all_files,i);
          break;
        }
      }
  } else {
      free(verb_now);
      return -1;
  }
  free(verb_now);
  return 0;

}

void read_header(client_info *cur_client){
    //tell what kind of request we are getting
    header_flag  = 0;
    char *buffer  = cur_client->header + cur_client->offset;
    int   nread   = get_header(cur_client, buffer, 1024 - cur_client->offset);

    if (nread == -1) {
        do_error(cur_client, err_bad_request);
        return;
    }

    if (header_flag == 0) {
        cur_client->offset += nread;
    } else if (header_flag == 1) {
        if (parse_header(cur_client) == -1) {
            do_error(cur_client, err_bad_request);
        }
    }
}

void do_use_fd(int client_sock){
    /* frist try to find the corresboding client, then give client different states based on the request.
      This function will try to finish as many tasks as possible.
      However, if it is interuptted, it will resume and finish the correct states
      Update states when necessary!!!!!
    */
    int find = 0;
    client_info* cur_client = NULL;
    for(size_t i= 0; i < vector_size(sockets); i++){
      int temp = *((int*)vector_get(sockets, i));
      if(temp == client_sock){
        find = 1;
        cur_client = (client_info*)vector_get(clients, i);
        break;
      }
    }
    if (!find) {
        fprintf(stderr, "ERROR: client_state does not exist \n");
        exit(1);
    }

    if (cur_client->state == READING_HEADER) {
        read_header(cur_client);
        if (cur_client->request == DELETE) {
            do_delete(cur_client);
        } else if (cur_client->request == LIST) {
            do_list(cur_client);
        } else if (cur_client->request == GET) {
            do_get(cur_client);
        }
    } else if (cur_client->state == READING_FROM_CLIENT) {
        do_put(cur_client);
        if(cur_client->state == WRITING_ON_SERVER){
          write_at_server(cur_client);
        }
    } else if(cur_client->state == WRITING_ON_SERVER){
      write_at_server(cur_client);
    } else if (cur_client->state == WRITING_OK) {
        write_response_ok(cur_client);
          if (cur_client->state == LIST_WRITEBACK) {
              list_writeback(cur_client);
          } else if (cur_client->state == GET_WRITEBACK) {
              get_writeback(cur_client);
          }
    } else if (cur_client->state == LIST_WRITEBACK) {
          list_writeback(cur_client);
      } else if (cur_client->state == GET_WRITEBACK) {
          get_writeback(cur_client);
      }else if (cur_client->state == WRITING_ERR) {
        write_response_error(cur_client);
        if (cur_client->state == WRITING_ERRMSG) {
            write_response_errormsg(cur_client);
        }
    } else if(cur_client->state == WRITING_ERRMSG) {
        write_response_errormsg(cur_client);
    }
}

void free_all(){
  for(size_t i = 0; i < vector_size(all_files); i++){
      char* cur = (char*)(vector_get(all_files,i));
      delete_file(cur);
   }
  rmdir(directory);
  vector_destroy(all_files);
  for(size_t i = 0; i < vector_size(clients); i++){
      client_info* cur = (client_info*)(vector_get(clients,i));
      free(cur->filename);
      free(cur->header);
      free(cur->write_back_buffer);
      free((char*)cur->err_msg);
      free(cur->client_file);
      free(cur);
  }

  vector_destroy(clients);
  vector_destroy(sockets);
  close(epollfd);
}
void close_server(){
  //end the while loop, free and close everything
    end = 1;
    free_all();
}

void sp_handler() {
  // do nothing on sigpipe
    return;
}

int main(int argc, char **argv){
    if (argc != 2) {
        print_server_usage();
        return -1;
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = close_server;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction error");
        return -1;
    }
    signal(SIGPIPE, sp_handler);

    char template[] = "XXXXXX";
    directory = mkdtemp(template);
    if (directory == NULL) {
      perror("mkdtemp");
      exit(1);
    }
    print_temp_directory(directory);

    char *port = argv[1];
    //used codes from man page below
    int listen_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;
    int info_result = getaddrinfo(NULL, port, &hints, &result);
    if (info_result) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(info_result));
        exit(1);
    }

    int optval = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (bind(listen_sock, result->ai_addr, result->ai_addrlen)) {
        perror("bind()");
        exit(1);
    }

    if (listen(listen_sock, MAX_CLIENTS)) {
        perror("listen()");
        exit(1);
    }

    freeaddrinfo(result);

    //used codes from epoll man page below
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1()");
        exit(1);
    }

    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
    ev.events  = EPOLLIN;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(1);
    }

    int nfds;

    all_files = shallow_vector_create();
    clients = shallow_vector_create();
    sockets = int_vector_create();
    max_filesize = sizeof(size_t);
    client_info *clients_storage[MAX_CLIENTS];
    int sockets_storage[MAX_CLIENTS];
    int index = 0;
    end = 0;
    while (!end) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(1);
        }
        for (int n=0; n<nfds; n++) {
            if (events[n].data.fd == listen_sock) {
                int client_sock = accept(listen_sock, NULL, NULL);
                if (client_sock == -1) {
                    perror("accept");
                    exit(1);
                }
                int temp = fcntl(client_sock, F_GETFL, 0);
                fcntl(client_sock, F_SETFL, temp | O_NONBLOCK);
                //initialize new clients
                client_info* incoming_client = calloc(1,sizeof(client_info));
                new_clients(client_sock, incoming_client);
                sockets_storage[index] = client_sock;
                clients_storage[index] = incoming_client;
                index++;
                //add epoll event
                ev.events  = EPOLLIN;
                ev.data.fd = client_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, client_sock, &ev) == -1) {
                    perror("epoll_ctl: client_sock");
                    exit(1);
                }
            } else {
              //ready to process
              // God, I try to use events[n].data.ptr for the new clients.
              // I dont know why it does not work and I have been stucked here for so long.
              do_use_fd(events[n].data.fd);
            }
        }
        if(end == 1){
          break;
        }
    }
    return 0;
}

