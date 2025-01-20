#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <ctype.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>

#define SH_RL_BUFSIZE 1024
#define size_of_buff 1024
#define MAX_ARGS_SIZE 1024
#define BUFFER_SIZE 1024

#define ANSI_COLOR_GREEN   "\x1b[32;1m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34;1m"
#define ANSI_COLOR_MAGENTA "\x1b[35;1m"

void simplesh_clear();
void simplesh_history_last();
void simplesh_history(int no_args);

char history_name[1024], no_args_name[1024];
extern char **environ;
int par_pid, terminal_no;

typedef struct Msg {
    long mtype;
    char command[1000];
    char msg[6000];
    int pid;
    int terminal;
} Msg;

int cpl;

void to_exit(int qid, int cpl, char *command) {
    Msg msg;
    if (cpl == 1) {
        msg.msg[0] = '\0';
        strncpy(msg.command, command, sizeof(msg.command) - 1);
        msg.command[sizeof(msg.command) - 1] = '\0'; // Ensure null-termination
        printf("%s\n", command);
        msg.pid = par_pid;
        msg.mtype = 2;
        msg.terminal = terminal_no;
        int r;
        if (r=send_message(qid, &msg) == -1) {
            perror("Error while sending message (not uncoupled yet)");
            exit(EXIT_FAILURE);
        }
    }
    exit(EXIT_FAILURE);
}

int send_message(int qid, struct Msg *qbuf) {
    int result;
    size_t length;

    // Get the length of the message excluding the 'mtype' field
    length = sizeof(struct Msg) - sizeof(long);

    // Send the message to the queue
    result = msgsnd(qid, qbuf, length, 0);
    if (result == -1) {
        perror("msgsnd failed");
        return -1;
    }

    return result;
}


int read_message(int qid, long type, struct Msg *qbuf) {
    int result, length;

    length = sizeof(struct Msg) - sizeof(long);
    if ((result = msgrcv(qid, qbuf, length, type, 0)) == -1) {
        return -1;
    }
    return result;
}

void simplesh_env() {
    char **env = environ;
    while (*env) {
        printf("%s\n", *env);
        env++;
    }
}

void simplesh_pwd() {
    char buf[1024];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("Error in getcwd()");
    } else {
        printf("Current working directory: %s\n", buf);
    }
}

void simplesh_clear() {
    printf("\033[2J\033[1;1H");
}

char* simplesh_read() {
    char *line = NULL;
    ssize_t bufsize = 0;
    if (getline(&line, &bufsize, stdin) == -1) {
        perror("Error reading input");
        free(line);
        exit(EXIT_FAILURE);
    }
    return line;
}

void simplesh_history_last() {
    FILE *file1 = fopen(history_name, "r");
    if (!file1) {
        perror("Error opening the history file");
        return;
    }

    char *line = NULL;
    size_t len = 0;
    char *last_line = NULL;

    while (getline(&line, &len, file1) != -1) {
        if (last_line) {
            free(last_line);
        }
        last_line = strdup(line); // Store the last line
    }

    if (last_line) {
        printf("%s", last_line);
        free(last_line);
    }

    fclose(file1);
    free(line);
}

void simplesh_history(int no_args) {
    if (no_args <= 0) {
        printf("Invalid option\n");
        return;
    }

    FILE *file1 = fopen(history_name, "r");
    if (!file1) {
        perror("Error opening the history file");
        return;
    }

    char *line = NULL;
    size_t len = 0;
    int count = 0;

    while (getline(&line, &len, file1) != -1) {
        count++;
    }

    fseek(file1, 0, SEEK_SET);
    int skip_lines = count > no_args ? count - no_args : 0;
    count = 0;

    while (getline(&line, &len, file1) != -1) {
        if (count++ >= skip_lines) {
            printf("%s", line);
        }
    }

    fclose(file1);
    free(line);
}

char** parse_command(char* line, int *cnt, int *bg) {
    char buf[BUFFER_SIZE];
    char **words = malloc(MAX_ARGS_SIZE * sizeof(char*));
    int state = 0, i = 0, j = 0, k = 0;

    while (line[i] != '\0') {
        switch (state) {
            case 0:
                if (line[i] == '"') state = 2;
                else if (!isspace(line[i])) state = 1, buf[j++] = line[i];
                break;
            case 1:
                if (line[i] == '"') state = 2;
                else if (!isspace(line[i])) buf[j++] = line[i];
                else {
                    if (j > 0 && buf[j - 1] == '&') {
                        *bg = 1;
                        buf[--j] = '\0';
                    }
                    buf[j] = '\0';
                    words[k++] = strdup(buf);
                    j = 0;
                    state = 0;
                }
                break;
            case 2:
                if (line[i] == '"') state = 3;
                else buf[j++] = line[i];
                break;
            case 3:
                if (line[i] == '"') state = 2;
                else if (!isspace(line[i])) state = 1, buf[j++] = line[i];
                else {
                    buf[j] = '\0';
                    words[k++] = strdup(buf);
                    j = 0;
                    state = 0;
                }
                break;
        }
        i++;
    }

    if (j > 0) {
        buf[j] = '\0';
        words[k++] = strdup(buf);
    }

    *cnt = k;
    return words;
}
void print_prompt()
{
    printf(ANSI_COLOR_GREEN "%s> " ANSI_COLOR_RESET, getenv("PWD"));
    exit(0);
}


/*

void execute(char* command, char** args, int bg)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        int program = execvp(command, args);
        if (program == -1)
        {
            perror("Error running new program");
            exit(0);
        }
    }
    else
    {
        if (bg == 0)
        {
            waitpid(pid, NULL, 0);
        }
    }
}

void simplesh_cd(char* dir)
{
    if (dir == NULL)
    {
        char *home = getenv("HOME");
        if (home == NULL)
        {
            fprintf(stderr, "simplesh: cd: HOME not set\n");
            return;
        }
        if (chdir(home) != 0)
        {
            perror("simplesh: cd");
        }
    }
    else if (strcmp(dir, "..") == 0)
    {
        if (chdir("..") != 0)
        {
            perror("simplesh: cd");
        }
    }
    else
    {
        if (chdir(dir) != 0)
        {
            perror("simplesh: cd");
        }
    }
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("Current working directory: %s\n", cwd);
    }
    else
    {
        perror("simplesh: cd");
    }
}

void simplesh_mkdir(char* dir)
{
    if (mkdir(dir, 0777) == 0) {
        printf("Directory '%s' created successfully\n", dir);
    } else {
        perror("simplesh: mkdir");
    }
}

void simplesh_rmdir(char* dir)
{
    if (rmdir(dir) == 0) {
        printf("Directory '%s' removed successfully\n", dir);
    } else {
        perror("simplesh: rmdir");
    }
}

void *input(int qid) {
    Msg msg ;
    char *line, **args;
     strcpy(history_name, getenv("HOME"));
    strcpy(no_args_name, getenv("HOME"));
    strcat(history_name, "/.history");
    strcat(no_args_name, "/.no_args");
    int cpl=0,flag=0;
    while (1) {
        if(flag==0)
        {
        printf(ANSI_COLOR_GREEN "%s> " ANSI_COLOR_RESET, getenv("PWD"));
        }
        else{
            flag=0;
        }


        line = simplesh_read();
        int bg = 0, cnt = 0;
        args = parse_command(line, &cnt, &bg);

        if (cnt == 0) {
            free(line);
            continue;
        }

        char *command = args[0];
        if(strcmp(command,"couple")==0)
        {
            if(cpl==1)
            {
                printf("It is already coupled ");
                continue;
            }
            else{
                msg.pid=par_pid;
                msg.mtype=1;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error adding the message ");
                    to_exit(qid,cpl,command);
                }
                cpl=0;
                flag=1;
                continue;
            }
        }
        else if (strcmp(command, "history") == 0) {
            if (cnt == 1) {
                simplesh_history_last();
            } else {
                int n_arg = atoi(args[1]);
                simplesh_history(n_arg);
            }

            if(cpl==1)
            {
                strcpy(msg.command,command);
                msg.mtype=3;
                msg.terminal=terminal_no;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending msg");
                    to_exit(qid,cpl,command);
                }
            }
            continue;
        }
        else if( strcmp(command,"uncouple")==0)
        {
            if(cpl==0)
            {
                printf("terminal is already uncoupled");
                
            }
            else{
                cpl=0;
                msg.pid=par_pid;
                msg.mtype=2;
                msg.terminal=terminal_no;
                strcpy(msg.command,command);
                if(send_message(qid,&msg)==-1)
                {
                    perror("error encounered while sending the message");
                    to_exit(qid,cpl,command);
                }

            }
            continue;
        }
        else if (strcmp(command, "clear") == 0) {
            simplesh_clear();
            if(cpl==1)
            {
                strcpy(msg.command,command);
                msg.mtype=3;
                msg.terminal=terminal_no;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending msg");
                    to_exit(qid,cpl,command);
                }
            }
            continue;
        }
        else if (strcmp(command, "env") == 0) {
            simplesh_env();
            if(cpl==1)
            {
                strcpy(msg.command,command);
                msg.mtype=3;
                msg.terminal=terminal_no;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending msg");
                    to_exit(qid,cpl,command);
                }
            }
            continue;
        }
        else if (strcmp(command, "pwd") == 0) {
            simplesh_pwd();
            if(cpl==1)
            {
                strcpy(msg.command,command);
                msg.mtype=3;
                msg.terminal=terminal_no;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending msg");
                    to_exit(qid,cpl,command);
                }
            }
            continue;
        }
        else if (strcmp(command, "cd") == 0) {
            if (cnt <= 1) {
                simplesh_cd(NULL);
            } else {
                simplesh_cd(args[1]);
            }
            if(cpl==1)
            {
                strcpy(msg.command,command);
                msg.mtype=3;
                msg.terminal=terminal_no;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending msg");
                    to_exit(qid,cpl,command);
                }
            }
            continue;
        }
        else if (strcmp(command, "mkdir") == 0) {
            if (cnt <= 1) {
                perror("Please mention a directory to create");
            } else {
                for (int i = 1; i < cnt; i++) {
                    simplesh_mkdir(args[i]);
                }
            }
            if(cpl==1)
            {
                strcpy(msg.command,command);
                msg.mtype=3;
                msg.terminal=terminal_no;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending msg");
                    to_exit(qid,cpl,command);
                }
            }
        }
        else if (strcmp(command, "rmdir") == 0) {
            if (cnt <= 1) {
                perror("Please mention directories to remove");
            } else {
                for (int i = 1; i < cnt; i++) {
                    simplesh_rmdir(args[i]);
                }
            }
            if(cpl==1)
            {
                strcpy(msg.command,command);
                msg.mtype=3;
                msg.terminal=terminal_no;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending msg");
                    to_exit(qid,cpl,command);
                }
            }
        }
        else if (strcmp(command, "exit") == 0) {
            
            if(cpl==1)
            {
                strcpy(msg.command,command);
                msg.msg[0]='\0';
                msg.mtype=2;
                msg.terminal=terminal_no;
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending msg");
                    to_exit(qid,cpl,command);
                }
            }
            cpl=0;
            exit(0);
            continue;
        }
        else {
            execute(command, args, bg);
        
        }

        int pipes[2];
        if(pipe(pipes)==-1)
        {
         perror("error creating a pipe");
         exit(EXIT_FAILURE);
        }
        pid_t child_pid=fork();
        if(child_pid==0)
        {
            close(pipes[0]);
            dup2(pipes[1],STDOUT_FILENO);
            dup2(pipes[1],STDERR_FILENO);
            close(pipes[1]);
            system(command);
            exit(0);
        }
        int status;
        wait(&status);
        close(pipes[1]);

        if(bg==0)
        {
            close(pipes[1]);
            char buf[BUFFER_SIZE]={'\0'};
            int read_m=read(pipes[0],buf,BUFFER_SIZE);
            close(pipes[0]);
            if(read_m==-1)
            {
                perror("error with reading the stdout");
            }
            else if(read_m==0)
            {
                strcpy(msg.msg,'\0');
            }
            else{
                strcpy(msg.msg,buf);
            }
            strcpy(msg.command,command);
            msg.terminal=terminal_no;
            msg.mtype=3;
            if(cpl==1)
            {
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending the msg");
                    to_exit(qid,cpl,command);
                }
            }


        }
        else{
            int c_pid=fork();
            if(c_pid==0)
            {
                char buf[BUFFER_SIZE];
                int read_m=read(pipes[0],buf,BUFFER_SIZE);
                close(pipes[0]);
                if(read_m==-1)
                {
                    perror("error while reading the output thru pipes ");
                }
                else if(read==0)
                {
                    strcpy(msg.msg,'\0');
                }
                else{
                    strcpy(msg.msg,buf);
                }
                strcpy(msg.command,command);
                msg.terminal=terminal_no;
                msg.mtype=3;
                msg.pid=getppid();
                if(cpl==1)
                {
                if(send_message(qid,&msg)==-1)
                {
                    perror("error while sending the message");
                    to_exit(qid,cpl, command);

                }
                }
                exit(0);
            }
        }


        for (int i = 0; i < cnt; i++) {
            free(args[i]);
        }
        free(args);
        free(line);
        pthread_exit(NULL);
    }
}


void *output(int qid)
{
    Msg msg;
    while(1)
    {
        if(read_message(qid,par_pid,&msg)==-1)
        {
            perror("error while reading the msg");
            char str[10]="";
            to_exit(qid,cpl,str);
        }
        if(msg.pid==-1)
        {
            terminal_no=msg.terminal;
            printf(ANSI_COLOR_BLUE"ID :%d \n"ANSI_COLOR_RESET, msg.terminal);

        }
        else{
            printf(ANSI_COLOR_MAGENTA"\nTerminal %d: "ANSI_COLOR_RESET, msg.terminal);
            printf("%s", msg.command);
            printf("%s", msg.msg);
        }
        print_prompt();
        fflush(stdout);
    }
    pthread_exit(NULL);
}

int main() {
   par_pid=getpid();
    key_t key=1234;
    int qid=msgget(key,IPC_CREAT|0666);
    printf("QID:%d",qid);
    pthread_t thread1,thread2;
    int rc;
    rc=pthread_create(&thread1,NULL,input,qid);
    if(rc==-1)
    {
        printf("error creating thread1");
        exit(-1);
    }

    rc=pthread_create(&thread2,NULL,output,qid);
    if(rc==-1)
    {
        printf("error creating thread2");
        exit(-1);
    }
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    return 0;
}
*/

void execute(char* command, char** args, int bg)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        int program = execvp(command, args);
        if (program == -1)
        {
            perror("Error running new program");
            exit(0);
        }
    }
    else
    {
        if (bg == 0)
        {
            waitpid(pid, NULL, 0);
        }
    }
}

void simplesh_cd(char* dir)
{
    if (dir == NULL)
    {
        char *home = getenv("HOME");
        if (home == NULL)
        {
            fprintf(stderr, "simplesh: cd: HOME not set\n");
            return;
        }
        if (chdir(home) != 0)
        {
            perror("simplesh: cd");
        }
    }
    else if (strcmp(dir, "..") == 0)
    {
        if (chdir("..") != 0)
        {
            perror("simplesh: cd");
        }
    }
    else
    {
        if (chdir(dir) != 0)
        {
            perror("simplesh: cd");
        }
    }
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("Current working directory: %s\n", cwd);
    }
    else
    {
        perror("simplesh: cd");
    }
}

void simplesh_mkdir(char* dir)
{
    if (mkdir(dir, 0777) == 0) {
        printf("Directory '%s' created successfully\n", dir);
    } else {
        perror("simplesh: mkdir");
    }
}

void simplesh_rmdir(char* dir)
{
    if (rmdir(dir) == 0) {
        printf("Directory '%s' removed successfully\n", dir);
    } else {
        perror("simplesh: rmdir");
    }
}
void *input(void *arg) {
    int qid = (int)(intptr_t)arg; 
    printf(qid,"\n");
    /*if (arg == NULL) {
    perror("Invalid argument passed to thread");
    pthread_exit(NULL);
}*/
    //int qid = *(int *)arg; 
    
    Msg msg;
    char *line, **args;
    strcpy(history_name, getenv("HOME"));
    strcpy(no_args_name, getenv("HOME"));
    strcat(history_name, "/.history");
    strcat(no_args_name, "/.no_args");
    int cpl = 0, flag = 0;
    while (1) {
        if (flag == 0) {
            printf(ANSI_COLOR_GREEN "%s> " ANSI_COLOR_RESET, getenv("PWD"));
        } else {
            flag = 0;
        }

        line = simplesh_read();
        int bg = 0, cnt = 0;
        args = parse_command(line, &cnt, &bg);

        if (cnt == 0) {
            free(line);
            continue;
        }

        char *command = args[0];
        if (strcmp(command, "couple") == 0) {
            if (cpl == 1) {
                printf("It is already coupled ");
                continue;
            } else {
                msg.pid = par_pid;
                msg.mtype = 1;
                if (send_message(qid, &msg) == -1) {
                    perror("error adding the message ");
                    to_exit(qid, cpl, command);
                }
                cpl = 1;  // Assuming `cpl` is meant to be set to 1 here
                flag = 1;
                
            }
            continue;
        } else if (strcmp(command, "history") == 0) {
            if (cnt == 1) {
                simplesh_history_last();
            } else {
                int n_arg = atoi(args[1]);
                simplesh_history(n_arg);
            }

            if (cpl == 1) {
                strcpy(msg.command, command);
                msg.mtype = 3;
                msg.terminal = terminal_no;
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending msg");
                    to_exit(qid, cpl, command);
                }
            }
            continue;
        } else if (strcmp(command, "uncouple") == 0) {
            if (cpl == 0) {
                printf("terminal is already uncoupled");
            } else {
                cpl = 0;
                msg.pid = par_pid;
                msg.mtype = 2;
                msg.terminal = terminal_no;
                strcpy(msg.command, command);
                if (send_message(qid, &msg) == -1) {
                    perror("error encountered while sending the message");
                    to_exit(qid, cpl, command);
                }
            }
            continue;
        } else if (strcmp(command, "clear") == 0) {
            simplesh_clear();
            if (cpl == 1) {
                strcpy(msg.command, command);
                msg.mtype = 3;
                msg.terminal = terminal_no;
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending msg");
                    to_exit(qid, cpl, command);
                }
            }
            continue;
        } else if (strcmp(command, "env") == 0) {
            simplesh_env();
            if (cpl == 1) {
                strcpy(msg.command, command);
                msg.mtype = 3;
                msg.terminal = terminal_no;
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending msg");
                    to_exit(qid, cpl, command);
                }
            }
            continue;
        } else if (strcmp(command, "pwd") == 0) {
            simplesh_pwd();
            if (cpl == 1) {
                strcpy(msg.command, command);
                msg.mtype = 3;
                msg.terminal = terminal_no;
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending msg");
                    to_exit(qid, cpl, command);
                }
            }
            continue;
        } else if (strcmp(command, "cd") == 0) {
            if (cnt <= 1) {
                simplesh_cd(NULL);
            } else {
                simplesh_cd(args[1]);
            }
            if (cpl == 1) {
                strcpy(msg.command, command);
                msg.mtype = 3;
                msg.terminal = terminal_no;
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending msg");
                    to_exit(qid, cpl, command);
                }
            }
            continue;
        } else if (strcmp(command, "mkdir") == 0) {
            if (cnt <= 1) {
                perror("Please mention a directory to create");
            } else {
                for (int i = 1; i < cnt; i++) {
                    simplesh_mkdir(args[i]);
                }
            }
            if (cpl == 1) {
                strcpy(msg.command, command);
                msg.mtype = 3;
                msg.terminal = terminal_no;
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending msg");
                    to_exit(qid, cpl, command);
                }
            }
            continue;
        } else if (strcmp(command, "rmdir") == 0) {
            if (cnt <= 1) {
                perror("Please mention directories to remove");
            } else {
                for (int i = 1; i < cnt; i++) {
                    simplesh_rmdir(args[i]);
                }
            }
            if (cpl == 1) {
                strcpy(msg.command, command);
                msg.mtype = 3;
                msg.terminal = terminal_no;
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending msg");
                    to_exit(qid, cpl, command);
                }
            }

            continue;
        } else if (strcmp(command, "exit") == 0) {
            if (cpl == 1) {
                strcpy(msg.command, command);
                msg.msg[0] = '\0';
                msg.mtype = 2;
                msg.terminal = terminal_no;
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending msg");
                    to_exit(qid, cpl, command);
                }
            }
            cpl = 0;
            exit(0);
            continue;
        } else {
            execute(command, args, bg);
        }

        int pipes[2];
        if (pipe(pipes) == -1) {
            perror("error creating a pipe");
            exit(EXIT_FAILURE);
        }
        pid_t child_pid = fork();
        if (child_pid == 0) {
            close(pipes[0]);
            dup2(pipes[1], STDOUT_FILENO);
            dup2(pipes[1], STDERR_FILENO);
            close(pipes[1]);
            system(command);
            exit(0);
        }
        int status;
        wait(&status);
        close(pipes[1]);

        if (bg == 0) {
            close(pipes[1]);
            char buf[BUFFER_SIZE] = {'\0'};
            int read_m = read(pipes[0], buf, BUFFER_SIZE);
            close(pipes[0]);
            if (read_m == -1) {
                perror("error with reading the stdout");
            } else if (read_m == 0) {
                strcpy(msg.msg, "");
            } else {
                strcpy(msg.msg, buf);
            }
            strcpy(msg.command, command);
            msg.terminal = terminal_no;
            msg.mtype = 3;
            if (cpl == 1) {
                if (send_message(qid, &msg) == -1) {
                    perror("error while sending the msg");
                    to_exit(qid, cpl, command);
                }
            }
        } else {
            int c_pid = fork();
            if (c_pid == 0) {
                char buf[BUFFER_SIZE];
                int read_m = read(pipes[0], buf, BUFFER_SIZE);
                close(pipes[0]);
                if (read_m == -1) {
                    perror("error while reading the output through pipes");
                } else if (read_m == 0) {
                    strcpy(msg.msg, "");
                } else {
                    strcpy(msg.msg, buf);
                }
                strcpy(msg.command, command);
                msg.terminal = terminal_no;
                msg.mtype = 3;
                msg.pid = getppid();
                if (cpl == 1) {
                    if (send_message(qid, &msg) == -1) {
                        perror("error while sending the message");
                        to_exit(qid, cpl, command);
                    }
                }
                exit(0);
            }
        }

        for (int i = 0; i < cnt; i++) {
            free(args[i]);
        }
        free(args);
        free(line);
        pthread_exit(NULL);
    }
}

void *output(void *arg) {
    int qid = (int)(intptr_t)arg; 
    /*if (arg == NULL) {
    perror("Invalid argument passed to thread");
    pthread_exit(NULL);
}*/
    //int qid = *(int *)arg; 
    Msg msg;
    while (1) {
        if (read_message(qid, par_pid, &msg) == -1) {
            perror("error while reading the msg");
            char str[10] = "";
            to_exit(qid, cpl, str);
        }
        if (msg.pid == -1) {
            terminal_no = msg.terminal;
            printf(ANSI_COLOR_BLUE "ID :%d \n" ANSI_COLOR_RESET, msg.terminal);
        } else {
            printf(ANSI_COLOR_MAGENTA "\nTerminal %d: " ANSI_COLOR_RESET, msg.terminal);
            printf("%s", msg.command);
            printf("%s", msg.msg);
        }
        print_prompt();
        fflush(stdout);
    }
    pthread_exit(NULL);
}
int main() {
    par_pid = getpid();
    key_t key = 1234;

    // Get message queue id
    int qid = msgget(key, IPC_CREAT | 0666);
    if (qid == -1) {
        perror("Error creating or accessing message queue");
        exit(-1);
    }

    printf("QID: %d\n", qid);

    pthread_t thread1, thread2;
    int rc;

    // Create input thread
    rc = pthread_create(&thread1, NULL, input, (void *)(intptr_t)qid);
    if (rc != 0) {
        printf("Error creating thread1\n");
        exit(-1);
    }

    // Create output thread
    rc = pthread_create(&thread2, NULL, output, (void *)(intptr_t)qid);
    if (rc != 0) {
        printf("Error creating thread2\n");
        exit(-1);
    }

    // Join threads
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // Clean up message queue
    

    return 0;
}