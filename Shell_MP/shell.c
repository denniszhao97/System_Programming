/**
 * Shell Lab
 * CS 241 - Spring 2019
 */
#include "format.h"
#include "shell.h"
#include "vector.h"
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

extern char *optarg;
extern int optind;

typedef struct process {
    char *command;
    pid_t pid;
} process;

static process foreground_processes[100];
static process background_processes[100];
static int foreground_counter = 0;
static int background_counter = 0;

void kill_foreground() {
  int i = 0;
  for(; i < foreground_counter; i++){
      kill(foreground_processes[i].pid, SIGINT);
  }
}

void add_history(vector *historys, char *command){
    vector_push_back(historys, command);
    return;
}

int execute_buildin(char* cur_command, vector *historys){
   if(*cur_command == 'c' && cur_command[1] == 'd'){
      add_history(historys, cur_command);
      if(strlen(cur_command) <= 3){
         print_invalid_command(cur_command);
         return 1;
      }
      char* path = malloc(100);
      strcpy(path, cur_command + 3);
      if(chdir(path) == -1){
        print_no_directory(path);
      }
      free(path);
      return 1;
   }

   char *history_buildin = "!history";
   if(!strcmp(history_buildin, cur_command)){
    for(size_t i = 0; i < vector_size(historys); i++){
      print_history_line(i, vector_get(historys,i));
     }
    return 1;
   }

   if(*cur_command == '#'){
    int n;
    sscanf(cur_command+1, "%d", &n);
    if(n >= 0 && n < (int)vector_size(historys)){
      return (n+2);
    } else {
      print_invalid_index();
    }
    return 1;
   }

   if(*cur_command == '!'){
    char* temp = cur_command + 1;
    if(strlen(temp) == 0){
      return -(vector_size(historys)-1)-1;
    }
     int i = (int)vector_size(historys) -1;
    for(i = (int)vector_size(historys) - 1; i >= 0; i--){
      if(strstr(vector_get(historys,i),temp)!=NULL){
          return -i-1;
      }
    }
    print_no_history_match();
    return 1;
   }

   if(strlen(cur_command) >= 4){
     char *stop_char = malloc(5*sizeof(char));
     strncpy(stop_char, cur_command, 4);
     stop_char[4] = 0;
     if(!strcmp(stop_char, "stop")){
       if(strlen(cur_command) < 6){
         print_invalid_command(cur_command);
         free(stop_char);
         return 1;
       }
       add_history(historys, cur_command);
       int pid = 0;
       sscanf(cur_command+5, "%d", &pid);
       if(kill(pid,0) == -1){
         print_no_process_found(pid);
         free(stop_char);
         return 1;
       }
       if(kill(pid, SIGTSTP) == -1){
         print_no_process_found(pid);
         free(stop_char);
         return 1;
       }
       int i = 0;
       for(; i < background_counter; i++){
         if(background_processes[i].pid == pid){
           print_stopped_process(pid, background_processes[i].command);
           break;
         }
       }
       free(stop_char);
       return 1;
     } else{
       free(stop_char);
     }
   }

   if(strlen(cur_command) >= 4){
     char *kill_char = malloc(5*sizeof(char));
     strncpy(kill_char, cur_command, 4);
     kill_char[4] = 0;
     if(!strcmp(kill_char, "kill")){
       if(strlen(cur_command) < 6){
         print_invalid_command(cur_command);
         free(kill_char);
         return 1;
       }
       add_history(historys, cur_command);
       int pid = 0;
       sscanf(cur_command+5, "%d", &pid);
       if(kill(pid,0) == -1){
         print_no_process_found(pid);
         free(kill_char);
         return 1;
       }
      if(kill(pid, SIGTERM) == -1){
        print_no_process_found(pid);
        free(kill_char);
        return 1;
      }
       int i = 0;
       for(; i < background_counter; i++){
           if(background_processes[i].pid == pid){
               print_killed_process(pid, background_processes[i].command);
               break;
             }
        }
       free(kill_char);
       return 1;
     } else {
       free(kill_char);
     }
   }

   if(strlen(cur_command) >= 4){
     char *cont_char = malloc(5*sizeof(char));
     strncpy(cont_char, cur_command, 4);
     cont_char[4] = 0;
     if(!strcmp(cont_char, "cont")){
       if(strlen(cur_command) < 6){
         print_invalid_command(cur_command);
         free(cont_char);
         return 1;
       }
       add_history(historys, cur_command);
       int pid = 0;
       sscanf(cur_command+5, "%d", &pid);
       if(kill(pid,0) == -1){
         print_no_process_found(pid);
         free(cont_char);
         return 1;
       }
       if(kill(pid, SIGCONT) == -1){
         free(cont_char);
         return 1;
       }
       free(cont_char);
       return 1;
     } else {
        free(cont_char);
      }
   }

   char *ps_buildin = "ps";
   if(!strcmp(ps_buildin, cur_command)){
    print_process_info_header();
    int i = 0;
    char path2[1024];
    snprintf(path2, 1024, "/proc/stat");
    FILE* btime;
    btime = fopen(path2, "r");
    size_t capacity1 = 0;
    char* line1 = NULL;
    int read1 = 0;
    unsigned long long boot_time = 0;
    while((read1 = getline(&line1, &capacity1, btime)) != -1){
       if(strstr(line1, "btime") != NULL){
          sscanf(line1+6, "%llu", &boot_time);
          break;
       }
    }
    fclose(btime);
    free(line1);
    for(; i < background_counter+1; i++){
      process_info *pinfo = malloc(sizeof(process_info));
      if(i < background_counter){
        if(kill(background_processes[i].pid,0) != -1){
          int length = strlen(background_processes[i].command);
          pinfo->command = malloc((length+2)*sizeof(char));
          strcpy(pinfo->command, background_processes[i].command);
          pinfo->command[length] = '&';
          pinfo->command[length + 1] = 0;
          pinfo->pid = background_processes[i].pid;
        } else {
          free(pinfo);
          continue;
        }
      } else {
          pinfo->command = malloc(8*sizeof(char));
          strcpy(pinfo->command, "./shell");
          pinfo->command[7] = 0;
          pinfo->pid = getpid();
      }
        char path[1024];
        pid_t pid = pinfo->pid;
        FILE* stat;
        snprintf(path, 1024, "/proc/%d/stat", pid);
        stat = fopen(path, "r");
        size_t capacity = 0;
        char* line = NULL;
        int read = 0;
       while((read = getline(&line, &capacity, stat)) != -1){
           line[read-1] = 0;
       }
        fclose(stat);
        char *token = strtok(line, " ");
        int index = 0;
        unsigned long long utime = 0;
        while(token){
          if(index == 2){ pinfo->state = *token; }
          if(index == 21){
           unsigned long long start_time;
           sscanf(token, "%llu", &start_time);
           start_time = start_time / sysconf(_SC_CLK_TCK);
           unsigned long long total_time = start_time + boot_time;
           time_t basic = (time_t)total_time;
           struct tm *temp = localtime(&basic);
           char *result = malloc(6*sizeof(char));
           time_struct_to_string(result, 6, temp);
           result[5] = 0;
           pinfo->start_str = result;
          }
          if(index == 19){
            long int nthreads;
            sscanf(token, "%ld", &nthreads);
            pinfo->nthreads = nthreads;
          }
          if(index == 13){ sscanf(token, "%llu", &utime); }
          if(index == 14){
            unsigned long long stime;
              sscanf(token, "%llu", &stime);
             unsigned long long total_time = (stime + utime) / sysconf(_SC_CLK_TCK);
              time_t basic = (time_t)total_time;
              struct tm *temp = localtime(&basic);
              size_t min = temp->tm_min;
              size_t sec = temp->tm_sec;
              char *result2 = malloc(6*sizeof(char));
              execution_time_to_string(result2,6,min,sec);
              pinfo->time_str = result2;
          }
          if(index == 22){
            unsigned long int vsize;
            sscanf(token, "%lu", &vsize);
            pinfo->vsize = vsize / 1024;
          }
          token = strtok(NULL, " ");
          index++;
        }
        print_process_info(pinfo);
        free(line);
        free(pinfo->start_str);
        free(pinfo->time_str);
        free(pinfo->command);
        free(pinfo);
     }
     return 1;
   }

   if(strlen(cur_command) >= 3){
     char *pfd_char = malloc(4*sizeof(char));
     strncpy(pfd_char, cur_command, 3);
     pfd_char[3] = 0;
     if(!strcmp(pfd_char, "pfd")){
       if(strlen(cur_command) < 5){
         print_invalid_command(cur_command);
         return 1;
       }
       free(pfd_char);
       print_process_fd_info_header();
       add_history(historys, cur_command);
       int pid = 0;
       sscanf(cur_command+4, "%d", &pid);
       if(kill(pid,0) == -1){
         print_no_process_found(pid);
         return 1;
       }
       int i = 0;
       while(i < 100){
         char expect_path[1024];
         snprintf(expect_path, 1024, "/proc/%d/fd/%d", pid, i);
         char *buffer = malloc(100*sizeof(char));
         memset(buffer,0,100);
         size_t buffer_size = 100;
         int read = readlink(expect_path, buffer, buffer_size);
         int l = strlen(buffer);
         buffer[l] = 0;
         if(read == -1){
           free(buffer);
           break;
         }
         char *real_path = NULL;
         real_path =  get_full_path(buffer);
         char expect_path2[1024];
         snprintf(expect_path2, 1024, "/proc/%d/fdinfo/%d", pid, i);
         FILE* loc;
         loc = fopen(expect_path2, "r");
         size_t capacity1 = 0;
         char* line1 = NULL;
         int read1 = 0;
         int file_loc = 0;
         while((read1 = getline(&line1, &capacity1, loc)) != -1){
            if(strstr(line1, "pos:") != NULL){
               sscanf(line1+4, "%d", &file_loc);
               break;
            }
         }
         fclose(loc);
         free(line1);
         int file_no = i;
         print_process_fd_info(file_no, file_loc, real_path);
         free(buffer);
         free(real_path);
         i++;
       }
       return 1;
     } else {
       free(pfd_char);
     }
   }
   return 0;
}
void sigchld_handler(){
  while (waitpid((pid_t) (-1), 0, WNOHANG) > 0);
  return;
}
void execute_notBuildin(char* cur_command, vector *historys, int background){
  pid_t pid = fork();
  if(pid < 0){
     print_fork_failed();
    exit(1);
  } else if (pid > 0) { //parents
    if (setpgid(pid, pid) == -1) {
      print_setpgid_failed();
      exit(1);
    }
    if(background){ // & background command
      process cur;
      cur.command = cur_command;
      cur.pid = pid;
      background_processes[background_counter] = cur;
      background_counter++;
      return;
    } else { //foreground command
        process cur;
        cur.command = cur_command;
        cur.pid = pid;
        foreground_processes[foreground_counter] = cur;
        foreground_counter++;
        int status;
        int finished = waitpid(pid, &status, 0);
          if(finished == -1){
             exit(1);
             return;
          }
         if(WEXITSTATUS(status) != 0){
           optind = 0;
           return;
         }
          return;
     }
  } else { // child
      add_history(historys, cur_command);
      char *non_buildin[1024];
      char *temp = strtok(cur_command, " ");
      int i = 0;
      while(temp != NULL){
          non_buildin[i] = temp;
          temp = strtok(NULL," ");
          i++;
      }
      non_buildin[i] = NULL;
      print_command_executed(getpid());
      int status = execvp(non_buildin[0], non_buildin);
      if(status < 0){
        print_exec_failed(cur_command);
        exit(1);
      }
  }
   return;
}
int hasOperator(char* cur_command){
  char *pattern1 = " && ";
  char *copy = malloc((strlen(cur_command)+1)*sizeof(char));
  strcpy(copy,cur_command);
  if(strstr(copy, pattern1)!=NULL){
    return 1;
  }
  free(copy);

  char *copy2 = malloc((strlen(cur_command)+1)*sizeof(char));
  strcpy(copy2,cur_command);
  char *pattern2 = " || ";
  if(strstr(copy2, pattern2)!=NULL){
    return 2;
  }
  free(copy2);

  char *copy3 = malloc((strlen(cur_command)+1)*sizeof(char));
  strcpy(copy3,cur_command);
  char *pattern3 = "; ";
  if(strstr(copy3, pattern3)!=NULL){
    return 3;
  }
  free(copy3);

  return 0;
}

void execute_command(char* cur_command, vector *historys, int background){
   int i = hasOperator(cur_command);
  if(i != 0){
     if(i == 1){
       optind = 1;
       add_history(historys, cur_command);
       char* token = " && ";
       int originl = strlen(cur_command);
       char *command2 = NULL;
      command2 = strstr(cur_command, token);
      int offset = originl - strlen(command2);
      char* command1 = malloc((offset+1)*sizeof(char));
      strncpy(command1,cur_command,offset);
      command1[offset] = 0;
      execute_command(command1, historys, background);
      if(optind == 1){
        command2 = command2 + 4;
        execute_command(command2, historys, background);
        vector_pop_back(historys);
        vector_pop_back(historys);
      } else {
       vector_pop_back(historys);
      }
      free(command1);
      return;
     } else if(i == 2){
       optind = 1;
       add_history(historys, cur_command);
       char* token = " || ";
       int originl = strlen(cur_command);
       char *command2 = NULL;
      command2 = strstr(cur_command, token);
      int offset = originl - strlen(command2);
      char* command1 = malloc((offset+1)*sizeof(char));
      strncpy(command1,cur_command,offset);
      command1[offset] = 0;
      execute_command(command1, historys, background);
      if(optind == 0){
        command2 = command2 + 4;
        execute_command(command2, historys, background);
        vector_pop_back(historys);
        vector_pop_back(historys);
      } else {
       vector_pop_back(historys);
     }
      free(command1);
      return;
     } else{
       add_history(historys, cur_command);
       char* token = "; ";
       int originl = strlen(cur_command);
       char *command2 = NULL;
      command2 = strstr(cur_command, token);
      int offset = originl - strlen(command2);
      char* command1 = malloc((offset+1)*sizeof(char));
      strncpy(command1,cur_command,offset);
      command1[offset] = 0;
      execute_command(command1, historys,background);
      command2 = command2 + 2;
      execute_command(command2, historys, background);
      vector_pop_back(historys);
      vector_pop_back(historys);
      free(command1);
       return;
     }
  }

  if((i = execute_buildin(cur_command, historys))){
    if(i > 1){ // #
      i = i - 2;
      cur_command = vector_get(historys,i);
      print_command(cur_command);
      execute_command(cur_command, historys, background);
      return;
    }
    if(i < 1){ // !
      i = -i-1;
      cur_command = vector_get(historys,i);
      print_command(cur_command);
      execute_command(cur_command, historys, background);
      return;
    }
     return;
  }
  add_history(historys, cur_command);
  execute_notBuildin(cur_command, historys, background);
}
int execute_command_file(char *command_filepath, vector *historys, vector *commands, char *start, pid_t pid){
  int background = 0;
   FILE *temp  = fopen(command_filepath, "r");
   size_t capacity = 0;
   char* line = NULL;
   int read = 0;
   while(1){
     if((read = getline(&line, &capacity, temp)) != -1){
        print_prompt(start,pid);
        line[read-1] = 0;
        if(line[read-2] == '&' && line[read-3] != '&'){
          background = 1;
          line[read-2] = 0;
        }
        vector_push_back(commands,line);
        int i = strcmp(line, "exit");
        if(i == 0){
          print_command(line);
          fclose(temp);
          free(line);
          return 1;
        }
        char* cur_command = vector_get(commands, vector_size(commands)-1);
        print_command(cur_command);
        execute_command(cur_command, historys, background);
   } else {
     fclose(temp);
     free(line);
     return 1;
   }
 }
}

int shell(int argc, char *argv[]) {
    // TODO: This is the entry point for your shell.
   signal(SIGINT,  kill_foreground);
   signal(SIGCHLD, sigchld_handler);

    if(argc != 1 && argc != 3 &&  argc != 5){
      print_usage();
      exit(0);
    }
    //flags
    int target = 0;
    int wait_flag = 0;
    int error_flag = 0;
    int history_flag = 0;
    int command_flag = 0;

    //file name and file path pointers
    char *history_filename = NULL;
    char *history_filepath = NULL;
    char *command_filename = NULL;
    char *command_filepath = NULL;
    vector *historys = string_vector_create();
    vector *commands = string_vector_create();
    // distinguish different types of commands
    while((target = getopt(argc, argv, "h:f:")) != -1){
       switch(target){
         case 'h':
            if(optarg && (history_flag != 1)){
              history_flag = 1;
              char *copy_string = malloc((strlen(optarg)+1)*sizeof(char));
              strcpy(copy_string, optarg);
              copy_string[strlen(optarg)] = 0;
              history_filename = copy_string;
            } else {
              print_usage();
              exit(0);
            }
            break;
         case 'f':
           if(optarg && (command_flag != 1)){
             command_flag = 1;
             char *copy_string2 = malloc((strlen(optarg)+1)*sizeof(char));
             strcpy(copy_string2, optarg);
             copy_string2[strlen(optarg)] = 0;
             command_filename = copy_string2;
           } else {
             print_usage();
             exit(0);
           }
           break;
       }
    }
    // read history file and command file
    if(history_flag == 1){
      if(access(history_filename, F_OK) == -1){
          print_history_file_error();
          FILE *temp = fopen(history_filename, "w");
          fclose(temp);
          history_filepath= get_full_path(history_filename);
        } else {
          history_filepath= get_full_path(history_filename);
          FILE *temp = fopen(history_filepath, "r");
          char *line = NULL;
          size_t n = 0;
          int length = 0;
          while((length = getline(&line, &n, temp)) != -1){
            line[length-1] = 0;
            vector_push_back(historys, line);
          }
          free(line);
          fclose(temp);
        }
        wait_flag = 1;
    }
   // main while loop
    while(1){
      int background = 0;
      pid_t pid = getpid();
      char* temp = malloc(100);
      size_t capacity = 100;
      int read = 0;
      char *start = temp;
      getcwd(start,100);
      while((argc == 1 || wait_flag == 1) && command_flag != 1){
            print_prompt(start, pid);
          if((read = getline(&temp, &capacity, stdin)) != -1){
            if(read == 0){
               vector_destroy(commands);
               vector_destroy(historys);
               free(start);
               exit(0);
            }
            temp[read-1] = 0;
            if(temp[read-2] == '&' && temp[read-3] != '&'){
              background = 1;
              temp[read-2] = 0;
            }
            vector_push_back(commands,temp);
            int i = strcmp(temp, "exit");
            if(i == 0){
               vector_destroy(commands);
               vector_destroy(historys);
               free(start);
                free(history_filename);
               free(command_filename);
              free(history_filepath);
               free(command_filepath);
               exit(0);
            }
            break;
         } else {
           vector_destroy(commands);
           vector_destroy(historys);
           free(start);
           free(history_filename);
           free(command_filename);
           free(history_filepath);
           free(command_filepath);
           exit(0);
         }
      }
      // execute commands
      if(command_flag == 1){
        if(access(command_filename, F_OK) == -1){
          print_script_file_error();
          vector_destroy(commands);
          vector_destroy(historys);
          free(history_filename);
         free(command_filename);
         free(history_filepath);
         free(command_filepath);
          free(start);
          exit(0);
        } else {
          command_filepath = get_full_path(command_filename);
        }
        error_flag = execute_command_file(command_filepath,historys,commands,start,pid);
      } else {
        char* cur_command = vector_get(commands, vector_size(commands)-1);
        execute_command(cur_command, historys, background);
      }
      //update historys
      if(history_flag == 1){
         FILE *history = fopen(history_filepath, "a");
         fprintf(history, "%s\n", vector_get(historys, vector_size(historys)-1));
         fclose(history);
      }
      if(error_flag == 1){
        vector_destroy(commands);
        vector_destroy(historys);
        free(start);
        free(history_filename);
        free(command_filename);
        free(history_filepath);
        free(command_filepath);
        return 0;
      }
      free(start);
    }

    return 0;
}

