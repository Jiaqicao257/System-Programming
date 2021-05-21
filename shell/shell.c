

#include "format.h"
#include "shell.h"
#include "vector.h"
#include "sstring.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct process {
    char *command;
    pid_t pid;
} process;

static vector *processes;
process* create(const char *buffer, pid_t pid);
void destroy();
void destroy_process(pid_t pid);
void killall();
void ctrl_and_c();
void background();
int handle_external_cmd(char *buffer);
void kill_process(pid_t pid);
void stop_process(pid_t pid);
void cont_process(pid_t pid);
process_info* handle_pi_create(char *command, pid_t pid);
void process_info_destroy(process_info *this);
void process_info_ps();
void process_fd_info_pfd(pid_t pid);
int shell(int argc, char *argv[]) {
    signal(SIGINT, ctrl_and_c);
    signal(SIGCHLD, background);
    processes = shallow_vector_create();
    vector *history = string_vector_create();
    int count = 1;
    int opt;
    char* h = NULL;
    char* c = NULL;
    while ((opt = getopt(argc, argv, "h:f:")) != -1) {
      switch(opt){
        case 'h': h = optarg; count+=2; break;
        case 'f': c = optarg; count+=2; break;
      }
    }
    if (argc != count) {
      print_usage();
      exit(1);
    }
    // load history
    FILE *file_h;
    char *path_h;
    if (h) {
        path_h = get_full_path(h);
        file_h = fopen(path_h, "r");
        if (!file_h) {
            print_history_file_error();
        } else {
            char *histories = NULL;
            size_t size_h = 0;
            ssize_t bytes;
            while (1) {
              bytes = getline(&histories,&size_h, file_h);
              if (bytes == -1) break;
              if (bytes>0 && histories[bytes-1] == '\n') {
                histories[bytes-1] = '\0';
                vector_push_back(history, histories);
              }
            }
            free(histories);
            fclose(file_h);
        }
    }
    // open file or stdin
    FILE *file;
    if (c) {
        file = fopen(c, "r");
        if (!file) {
            print_script_file_error();
            exit(1);
        }
    } else file = stdin;
    // shell loop
    char *buffer = NULL;
    size_t size = 0;
    ssize_t bytes_read;
    while (1) {
      char *full_path = get_full_path("./");
      print_prompt(full_path, getpid());
      free(full_path);
      // command input
      bytes_read = getline(&buffer,&size, file);
      if (bytes_read == -1){
        killall(); break;
      }
      if (bytes_read>0 && buffer[bytes_read-1] == '\n') {
        buffer[bytes_read-1] = '\0';
        if (file != stdin) print_command(buffer);
      }
      // week 2 built-in command
      if (!strcmp(buffer,"ps")) {
        process_info_ps();
      } else if (!strncmp(buffer,"pfd", 3)) {
        pid_t pdf_pid; size_t pdf_num_read;
        pdf_num_read = sscanf(buffer+3, "%d", &pdf_pid);
        if ( pdf_num_read != 1) {
          print_invalid_command(buffer);
        } else {
          process_fd_info_pfd(pdf_pid);
        }
      } else if (!strncmp(buffer,"kill", 4)) {
        pid_t kill_pid; size_t kill_num_read;
        kill_num_read = sscanf(buffer+4, "%d", &kill_pid);
        if ( kill_num_read != 1) {
          print_invalid_command(buffer);
        } else {
          kill_process(kill_pid);
        }
      } else if (!strncmp(buffer,"stop", 4)) {
        pid_t stop_pid; size_t stop_num_read;
        stop_num_read = sscanf(buffer+4, "%d", &stop_pid);
        if ( stop_num_read != 1) {
          print_invalid_command(buffer);
        } else {
          stop_process(stop_pid);
        }
      } else if (!strncmp(buffer,"cont", 4)) {
        pid_t cont_pid; size_t cont_num_read;
        cont_num_read = sscanf(buffer+4, "%d", &cont_pid);
        if ( cont_num_read != 1) {
          print_invalid_command(buffer);
        } else {
          cont_process(cont_pid);
        }
      }
      // built-in command
      else if (!strcmp(buffer,"!history")) {
          for (size_t i = 0; i < vector_size(history); i++) {
            print_history_line(i, (char *) vector_get(history, i));
          }
      } else if (buffer[0] == '#') {
          size_t num,num_read;
          num_read = sscanf(buffer+1, "%zu", &num);
          if (!num_read || num > vector_size(history)-1) {
            print_invalid_index();
          } else {
            char *his_cmd = (char *)vector_get(history, num);
            print_command(his_cmd);
            vector_push_back(history, his_cmd);
            handle_external_cmd(his_cmd);
          }
      } else if (buffer[0] == '!') {
          for (int i = vector_size(history) - 1; i >= 0 ; i--) {
            char *his_cmd = (char *)vector_get(history, i);
            if (buffer[1] == '\0' || !strncmp(buffer+1, his_cmd, strlen(buffer+1))) {
              print_command(his_cmd);
              vector_push_back(history, his_cmd);
              handle_external_cmd(his_cmd);
              break;
            }
            if (i == 0) print_no_history_match();
          }
      } else if (!strcmp(buffer,"exit")) {
          killall(); break;
      } else {
          vector_push_back(history, buffer);
          int flag = 0;
          sstring *buffers = cstr_to_sstring(buffer);
          vector *cmd = sstring_split(buffers, ' ');
          for (size_t i = 0; i < vector_size(cmd); i++) {
              char *op = (char *) vector_get(cmd, i);
              if (!strcmp(op, "&&")) {
                  char *buffer1 = strtok(buffer, "&");
                  buffer1[strlen(buffer1)-1] = '\0';
                  char *buffer2 = strtok(NULL, "");
                  buffer2 = buffer2+2;
                  if(!handle_external_cmd(buffer1)) {
                    handle_external_cmd(buffer2);
                  }
                  flag = 1;
              } else if (!strcmp(op, "||")) {
                  char *buffer1 = strtok(buffer, "|");
                  buffer1[strlen(buffer1)-1] = '\0';
                  char *buffer2 = strtok(NULL, "");
                  buffer2 = buffer2+2;
                  if(handle_external_cmd(buffer1)) {
                    handle_external_cmd(buffer2);
                  }
                  flag = 1;
              } else if (op[strlen(op)-1] == ';') {
                  char *buffer1 = strtok(buffer, ";");
                  char *buffer2 = strtok(NULL, "");
                  buffer2 = buffer2+1;
                  handle_external_cmd(buffer1);
                  handle_external_cmd(buffer2);
                  flag = 1;
              } else if (!strcmp(op, ">")) {
                    flag = 1;
                    char *buffer1 = strtok(buffer, ">");
                    buffer1[strlen(buffer1)-1] = '\0';
                    char *buffer2 = strtok(NULL, "");
                    buffer2 = buffer2+1;
                    char * redir_path = get_full_path(buffer2);
                    FILE* file_red = fopen(redir_path, "w");
                    if(!file_red) {print_redirection_file_error();}
                    else {
                        fflush(stdout);
                        int fff = fileno(file_red);
                        int f2 = fff;
                        dup2(fff, 1);
                        fclose(file_red);
                        handle_external_cmd(buffer1);
                        //FILE* red = fopen(redir_path, "r");
                        //dup2(fileno(red), f2);
                        dup2(1, f2);
                        free(redir_path);
                    }
              } else if (!strcmp(op, ">>")) {
                    flag = 1;
                    char *buffer1 = strtok(buffer, ">");
                    buffer1[strlen(buffer1)-1] = '\0';
                    char *buffer2 = strtok(NULL, "");
                    buffer2 = buffer2+1;
                    char * redir_path = get_full_path(buffer2);
                    FILE* file_red = fopen(redir_path, "a");
                    if(!file_red) {print_redirection_file_error();}
                    else {
                        fflush(stdout);
                        int fff = fileno(file_red);
                        dup2(fff, 1);
                        fclose(file_red);
                        free(redir_path);
                        handle_external_cmd(buffer1);
                    }
              } 

          }
          vector_destroy(cmd);
          sstring_destroy(buffers);
          if (!flag) {
            handle_external_cmd(buffer);
          }

        }
    }
    free(buffer);
    if (c) {
      fclose(file);
    }
    if (h) {
        FILE* f = fopen(path_h, "w");
        VECTOR_FOR_EACH(history, line, {
          fprintf(f, "%s\n", (char *)line);
        });
        fclose(f);
        free(path_h);
    }
    vector_destroy(history);
    return 0;
}

process* create(const char *buffer, pid_t pid) {
  process *this = calloc(1, sizeof(process));
  this->command = calloc(strlen(buffer)+1, sizeof(char));
  strcpy(this->command, buffer);
  this->pid = pid;
  return this;
}

void destroy() {
    for (size_t i = 0; i < vector_size(processes); i++) {
      process *this = (process *) vector_get(processes, i);
      free(this->command);
      free(this);
    }
    vector_destroy(processes);
}

void destroy_process(pid_t pid) {
  for (size_t i = 0; i < vector_size(processes); i++) {
    process *this = (process *) vector_get(processes, i);
    if ( this->pid == pid ) {
      free(this->command);
      free(this);
      vector_erase(processes, i);
      return;
    }
  }
}
void killall(){
  for (size_t i = 0; i < vector_size(processes); i++) {
      process *this = (process *) vector_get(processes, i);
      kill(this->pid, SIGKILL);
  }
  destroy();
}

void ctrl_and_c() {
  for (size_t i = 0; i < vector_size(processes); i++) {
    process *this = (process *) vector_get(processes, i);
    if ( this->pid != getpgid(this->pid) ){
      kill(this->pid, SIGKILL);
      destroy_process(this->pid);
    }
  }
}
void background() {
  pid_t pid;
  while ( (pid = waitpid(-1, 0, WNOHANG)) > 0) {
    destroy_process(pid);
  }
}
int handle_external_cmd(char *buffer) {
  if (!strncmp(buffer,"cd",2)) {
      int sucess = chdir(buffer+3);
      if (sucess < 0) {
        print_no_directory(buffer+3);
        return 1;
      } else return 0;
  } else{
        fflush(stdout);
        pid_t pid = fork();
        if (pid < 0) { 
            print_fork_failed();
            exit(1);
        } else if (pid > 0) { 
            process *this = create(buffer, pid);
            vector_push_back(processes, this);
            if (buffer[strlen(buffer)-1] == '&') {
                if (setpgid(pid, pid) == -1) {
                    print_setpgid_failed();
                    exit(1);
                }
            } else {
                if (setpgid(pid, getpid()) == -1) {
                    print_setpgid_failed();
                    exit(1);
                }
                int status;
                pid_t result_pid = waitpid(pid, &status, 0);
                if (result_pid != -1) {
                    destroy_process(result_pid);
                    if (WIFEXITED(status) && WEXITSTATUS(status)) {
                        return 1;
                    } 
                } else {
                  print_wait_failed();
                  exit(1);
                }
            }
        } else { 
            if (buffer[strlen(buffer)-1] == '&')
                buffer[strlen(buffer)-1] = '\0';
            vector *commands = sstring_split(cstr_to_sstring(buffer), ' ');
            char *command[vector_size(commands)+1];
            for (size_t i = 0; i < vector_size(commands); i++) {
              command[i] = (char *) vector_get(commands, i);
            }
            if (!strcmp(command[vector_size(commands)-1], ""))
              command[vector_size(commands)-1] = NULL;
            else
              command[vector_size(commands)] = NULL;
            print_command_executed(getpid());
            execvp(command[0], command);
            print_exec_failed(command[0]);
            exit(1);
        }
      }
      return 0;
}

void kill_process(pid_t pid){
  for (size_t i = 0; i < vector_size(processes); i++) {
    process *this = (process *) vector_get(processes, i);
    if ( this->pid == pid ){
      kill(this->pid, SIGKILL);
      print_killed_process(this->pid, this->command);
      destroy_process(this->pid);
      return;
    }
  }
  print_no_process_found(pid);
}
void stop_process(pid_t pid){
  for (size_t i = 0; i < vector_size(processes); i++) {
    process *this = (process *) vector_get(processes, i);
    if ( this->pid == pid ){
      kill(this->pid, SIGTSTP);
      print_stopped_process(this->pid, this->command);
      return;
    }
  }
  print_no_process_found(pid);
}
void cont_process(pid_t pid){
  for (size_t i = 0; i < vector_size(processes); i++) {
    process *this = (process *) vector_get(processes, i);
    if ( this->pid == pid ){
      kill(this->pid, SIGCONT);
      return;
    }
  }
  print_no_process_found(pid);
}
process_info* handle_pi_create(char *command, pid_t pid){
  process_info *this = calloc(1, sizeof(process_info));
  this->command = calloc(strlen(command)+1, sizeof(char));
  strcpy(this->command, command);
  this->pid = pid;
  char path[40], bufff[1000], *p;
  snprintf(path, 40, "/proc/%d/status", pid);
  FILE *status = fopen(path,"r");
  if (!status) {
      print_script_file_error();
      exit(1);
  }
  while(fgets(bufff, 100, status)) {
      if(!strncmp(bufff, "State:", 6)) {
        p = bufff + 7;
        while(isspace(*p)) ++p;
        this->state = *p;
      } else if (!strncmp(bufff, "Threads:", 8)) {
        char *ptr_thread;
        p = bufff + 9;
        while(isspace(*p)) ++p;
        this->nthreads = strtol(p, &ptr_thread, 10);
      } else if (!strncmp(bufff, "VmSize:", 7)) {
        char *ptr_vms;
        p = bufff + 8;
        while(isspace(*p)) ++p;
        this->vsize = strtol(p, &ptr_vms, 10);
      }
  }
  fclose(status);
  snprintf(path, 40, "/proc/%d/stat", pid);
  FILE *statf = fopen(path,"r");
  if (!statf) {
      print_script_file_error();
      exit(1);
  }
  fgets(bufff, 1000, statf);
  fclose(statf);
  p = strtok(bufff, " ");
  int i = 0;
  char *ptr_cpu;
  unsigned long utime, stime;
  unsigned long long starttime;
  while(p != NULL)
	{
    if (i == 13) {
      utime = strtol(p, &ptr_cpu, 10);
    } else if (i == 14) {
      stime = strtol(p, &ptr_cpu, 10);
    } else if (i == 21) {
      starttime = strtol(p, &ptr_cpu, 10);
    }
    p = strtok(NULL, " ");
		i++;
	}
  char buffer_cpu[100];
  unsigned long total_seconds_cpu = (utime+stime)/sysconf(_SC_CLK_TCK);
  if (!execution_time_to_string(buffer_cpu, 100, total_seconds_cpu/60, total_seconds_cpu%60)) exit(1);
  this->time_str = calloc(strlen(buffer_cpu)+1, sizeof(char));
  strcpy(this->time_str, buffer_cpu);
  FILE *statfsys = fopen("/proc/stat","r");
  if (!statfsys) {
      print_script_file_error();
      exit(1);
  }
  unsigned long btime;
  while(fgets(bufff, 100, statfsys)) {
      if(!strncmp(bufff, "btime", 5)) {
        p = bufff + 6;
        while(isspace(*p)) ++p;
        btime = strtol(p, &ptr_cpu, 10);
      }
  }
  fclose(statfsys);
  char buffer_start[100];
  time_t total_seconds_start = starttime/sysconf(_SC_CLK_TCK) + btime;
  struct tm *tm_info = localtime(&total_seconds_start);
  if (!time_struct_to_string(buffer_start, 100, tm_info)) exit(1);
  this->start_str = calloc(strlen(buffer_start)+1, sizeof(char));
  strcpy(this->start_str, buffer_start);
  return this;
}

void process_info_destroy(process_info *this) {
  free(this->start_str);
  free(this->time_str);
  free(this->command);
  free(this);
}

void process_info_ps(){
  print_process_info_header();
  for (size_t i = 0; i < vector_size(processes); i++) {
    process *proc = (process *) vector_get(processes, i);
    process_info *this = handle_pi_create(proc->command, proc->pid);
    print_process_info(this);
    process_info_destroy(this);
  }
  process_info *this_shell = handle_pi_create("./shell", getpid());
  print_process_info(this_shell);
  process_info_destroy(this_shell);
}

void process_fd_info_pfd(pid_t pid) {
  pid_t shell_pid = getpid();
  process *this = create("./shell", shell_pid);
  vector_push_back(processes, this);
  for (size_t i = 0; i < vector_size(processes); i++) {
    process *this = (process *) vector_get(processes, i);
    if (this->pid == pid) {
      char path[40];
      snprintf(path, 40, "/proc/%d/fdinfo", pid);
      DIR *fdinfo = opendir(path);
      if (!fdinfo) {
          print_script_file_error();
          exit(1);
      }
      print_process_fd_info_header();
      struct dirent *dent;
      while((dent=readdir(fdinfo))) {
        size_t fdinfo_num_read, fd_no;
        fdinfo_num_read = sscanf(dent->d_name, "%zu", &fd_no);
        if ( fdinfo_num_read == 1) {
          snprintf(path, 40, "/proc/%d/fdinfo/%zu", pid, fd_no);
          FILE *fdpos = fopen(path,"r");
          if (!fdpos) {
              print_script_file_error();
              exit(1);
          }
          char bufff[100], *p, *ptr; size_t file_pos;
          while(fgets(bufff, 100, fdpos)) {
              if(!strncmp(bufff, "pos:", 4)) {
                p = bufff + 4;
                while(isspace(*p)) ++p;
                file_pos = strtol(p, &ptr, 10);
              }
          }
          fclose(fdpos);
          char realpath[100]; ssize_t r;
          snprintf(path, 40, "/proc/%d/fd/%zu", pid, fd_no);
          if( (r = readlink(path, realpath, 99)) < 0) exit(1);
          else realpath[r] = '\0';
          print_process_fd_info(fd_no, file_pos, realpath);
        }

      }
      closedir(fdinfo);
      destroy_process(shell_pid);
      return;
    }
  }
  destroy_process(shell_pid);
  print_no_process_found(pid);
}


