#include "parser/ast.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/utsname.h>

#define BUFFERSIZE 100

volatile int running = 1;

void handle(node_t *n);

void sig_handler(int sig) {
    running = sig;
    if (sig == 2) {
        pid_t pid = fork();
        if (pid == 0)
            exit(0);
    }
    else if (sig == 18) {
        pid_t pid = fork();
        if (pid == 0)
            exit(0);
        setsid();
        chdir("/");
    }
}

void initialize(void)
{
    if (prompt)
        prompt = "vush$ ";
    signal(SIGINT, sig_handler);
    signal(SIGTSTP, sig_handler);
}

void run_command(node_t *node)
{
    signal(SIGINT, sig_handler);
    signal(SIGTSTP, sig_handler);
    
    if (prompt)
        prompt = "vush$ ";
    
    handle(node);
}

void handle_cmd(node_t *n) {
    char *program = n->command.program;
    char **argv = n->command.argv;
    if (strcmp(program, "exit") == 0) {
        if (n->command.argc > 1)
            _exit(atoi(argv[1]));
        _exit(0);
    }
    else if (strcmp(program, "cd") == 0) {
        if (chdir(argv[1]) < 0)
            perror("cd");
    }
    else if (strcmp(program, "mkdir") == 0) {
        if (mkdir(argv[1], 0777) < 0)
            perror("mdkir");
    }
    else if (strcmp(program, "/bin/pwd") == 0 || strcmp(program, "pwd") == 0) {
        char cwd[BUFFERSIZE];
        char *path = getcwd(cwd, sizeof(cwd));
        if (path == NULL)
            perror("pwd");
        else
            printf("%s\n", path);
    }
    else if (strcmp(program, "echo") == 0) {
        for (unsigned int i = 0; i < n->command.argc - 1; i++) {
            if (i > 0)
                write(1, " ", strlen(" "));
            write(1, argv[i + 1], strlen(argv[i + 1]));
        }
        write(1, "\n", strlen("\n"));
    }
    else if (strcmp(program, "uname") == 0) {
        struct utsname sys_info;
        if (uname(&sys_info) < 0)
            perror("uname");
        else
            printf("%s\n", sys_info.sysname);
    }
    else if (strcmp(program, "set") == 0) {
        if (putenv(argv[1]) < 0)
            perror("set");
        printf("%s\n", argv[1]);
    }
    else if (strcmp(program, "unset") == 0)
        unsetenv(argv[1]);
    else if (strcmp(program, "ls") == 0 || strcmp(program, "sleep") == 0) {
        pid_t pid = fork();
        if (pid < 0) {
            if (strcmp(program, "ls") == 0)
                perror("ls");
            else
                perror("sleep");
        }
        else if (pid == 0) {
            if (strcmp(program, "ls") == 0 && argv[1] != NULL)
                execl("/bin/ls", "ls", argv[1], argv[2], NULL);
            else if (strcmp(program, "ls") == 0)
                execl("/bin/ls", "ls", NULL);
            else {
                sleep(atoi(argv[1]));
            }
            exit(0);
        }
        else
            wait(NULL);
    }
    else {
        pid_t pid = fork();
        if (pid == 0) {
            if (execvp(program, argv) < 0)
                perror("command not found");
        }
        else if (pid < 0)
            perror("fork");
    }
}

void handle_pipe(node_t *n) {
    int p[2];
    pipe(p);
    pid_t pid1 = fork();
    if (pid1 == 0) {
        close(p[0]);
        close(0);
        dup2(p[1], 0);
        execvp(n->pipe.parts[1]->command.program, n->pipe.parts[1]->command.argv);
    }
    pid_t pid2 = fork();
    if (pid2 == 0) {
        close(p[1]);
        close(1);
        dup2(p[0], 0);
        execvp(n->pipe.parts[0]->command.program, n->pipe.parts[0]->command.argv);
    }
    close(p[0]);
    close(p[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

void handle_detach_cmd(node_t *n) {
    pid_t pid = fork();
    if (pid == 0) {
        handle_cmd(n);
        exit(0);
    }
    else if (pid < 0)
        perror("fork");
}

void handle_detach(node_t *n) {
    if (n->type == NODE_DETACH)
        n = n->detach.child;
    if (n->type == NODE_SEQUENCE) {
        handle_detach_cmd(n->sequence.first);
        if (n->sequence.second->type == NODE_COMMAND)
            handle_detach_cmd(n->sequence.second);
        else
            handle_detach(n->sequence.second);
    }
    else
        handle_detach_cmd(n);
}

void handle_sequence(node_t *n) {
    handle(n->sequence.first);
    handle(n->sequence.second);
}

void handle(node_t *n) {
    switch (n->type) {
        case NODE_COMMAND:
            handle_cmd(n);
            break;
        case NODE_PIPE:
            handle_pipe(n);
            break;
        case NODE_REDIRECT:
//             handle_redirect(n);
            break;
        case NODE_SUBSHELL:
//            handle_subshell(n);
            break;
        case NODE_SEQUENCE:
            handle_sequence(n);
            break;
        case NODE_DETACH:
            handle_detach(n);
            break;
    }
}
