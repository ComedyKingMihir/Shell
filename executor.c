/*****************/
/* Mihir M Celly */
/* 118647845     */
/* mcelly        */
/*****************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include "command.h"
#include "executor.h"
#define OPEN_FLAGS (O_WRONLY | O_TRUNC | O_CREAT)
#define DEF_MODE 0664

int execute(struct tree *t) {

   int status;
   int fd[2]; /* File descriptors for piping. */
   pid_t pid;

   if (t == NULL) {
      return 0; /* If the tree is empty, do nothing. */
   }

   status = 0;

   switch (t->conjunction) {
   case NONE:
   if (t->argv != NULL) {
      if (strcmp(t->argv[0], "exit") == 0) {
         exit(0); /* Simply exit the shell */
      } else if (strcmp(t->argv[0], "cd") == 0) {
         if (t->argv[1] != NULL) {
            if (chdir(t->argv[1]) != 0) {
               perror("cd"); /* Print error if chdir fails */
            }
         } else {
            /* If no directory is specified after "cd", go to the home directory */
            chdir(getenv("HOME"));
         }
      } else {
         pid = fork();
         if (pid == 0) { /* Child process. */
            /* Handle input redirection. */
            if (t->input != NULL) {
               int in_fd = open(t->input, O_RDONLY);
               if (in_fd == -1) {
                  perror("Failed to open input file");
                  exit(1);
               }
               dup2(in_fd, STDIN_FILENO);
               close(in_fd);
            }
            /* Handle output redirection. */
            if (t->output != NULL) {
               int out_fd = open(t->output, OPEN_FLAGS, DEF_MODE);
               if (out_fd == -1) {
                  perror("Failed to open output file");
                  exit(1);
               }
               dup2(out_fd, STDOUT_FILENO);
               close(out_fd);
            }
            execvp(t->argv[0], t->argv);
            fprintf(stderr, "Failed to execute %s\n", t->argv[0]);
            exit(1);
         } else if (pid > 0) { /* Parent process. */
            waitpid(pid, &status, 0);
         } else {
            perror("fork");
            return 1;
         }
      }
   }
   break;
   case AND:
      if (execute(t->left) == 0) {
         status = execute(t->right);
      } else {
         return 1; /* First command failed */
      }
      break;
   case PIPE:
      /* Check for ambiguous redirections first */
      if (t->left && t->left->output) {
         printf("Ambiguous output redirect.\n");
         return 1; /* Stop processing due to ambiguity in output */
      }
      if (t->right && t->right->input) {
         printf("Ambiguous input redirect.\n");
         return 1; /* Stop processing due to ambiguity in input */
      }
      if (pipe(fd) == -1) {
         perror("pipe");
         exit(1);
      }
      if ((pid = fork()) == -1) {
         perror("fork");
         exit(1);
      } else if (pid == 0) {
         close(fd[0]);
         dup2(fd[1], STDOUT_FILENO);
         close(fd[1]);
         exit(execute(t->left));
      } else {
         if ((pid = fork()) == -1) {
            perror("fork");
            exit(1);
         } else if (pid == 0) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            exit(execute(t->right));
         } else {
            close(fd[0]);
            close(fd[1]);
            waitpid(pid, &status, 0);
            wait(NULL); /* Wait for both children to exit. */
         }
      }
      break;
   case SUBSHELL:
      /* Handle subshell processing here */
      pid = fork();
      if (pid == 0) { /* Child process for subshell */
         status = execute(t->left);
         exit(status);
      } else if (pid > 0) { /* Parent process */
         waitpid(pid, &status, 0);
      } else {
         perror("fork");
         return 1;
      }
      break;
   default:
      fprintf(stderr, "Unsupported conjunction: %d\n", t->conjunction);
      return 1;
   }

   return status;
}