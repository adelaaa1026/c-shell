#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "jobs.h"

char buffer[1024];
char *tokens[1024];
char *arg[1024];
char *redirect[1024];
job_list_t *job_list;
int current_jid = 1;
int is_background = 0;

/*
 * parse the user input. The redirect symbols and the argument immediately
 * following them are placed in the redirect array. Other inputs go to the arg
 * array.
 */
int parse(char buffer[1024], char *tokens[1024], char *arg[1024],
          char *redirect[1024]) {
    char *str = buffer;
    char *token;
    int token_counter = 0;      // index of the token array
    int argv_counter = 0;       // index of the arg array
    int redirect_counter = 0;   // index of the redirect array
    int is_redirect_input = 0;  // indicate whether the previous token is <
    int is_redirect_output =
        0;                    // indicate whether the previous token is > or >>
    int redirect_input = 0;   // the number of < in total
    int redirect_output = 0;  // the number of > or >> in total

    while ((token = strtok(str, " \t\n")) != NULL) {
        tokens[token_counter] = token;
        is_background = 0;  // reset the flag
        if (!(strcmp(token, "<"))) {
            redirect[redirect_counter] = token;
            redirect_counter++;
            is_redirect_input = 1;  // set the flag
            redirect_input++;
        } else if (!(strcmp(token, ">")) || !(strcmp(token, ">>"))) {
            redirect[redirect_counter] = token;
            redirect_counter++;
            is_redirect_output = 1;  // set the flag
            redirect_output++;
        } else if (!(strcmp(token, "&"))) {
            is_background = 1;  // set the background process flag
        } else {
            if (is_redirect_input == 1 || is_redirect_output == 1) {
                redirect[redirect_counter] = token;
                redirect_counter++;
                is_redirect_input = 0;   // reset the input flag
                is_redirect_output = 0;  // reset the output flag
            } else {
                arg[argv_counter] = token;
                argv_counter++;
            }
        }
        token_counter++;
        str = NULL;
    }

    if (is_redirect_input == 1) {  // the input symbol isn't followed by a file
        fprintf(stderr, "syntax error: no input file\n");
        return -1;
    }
    if (is_redirect_output == 1) {  // the output symbol isn't followed by a
                                    // file
        fprintf(stderr, "syntax error: no output file\n");
        return -1;
    }
    if (redirect_input > 1) {
        fprintf(stderr, "syntax error: multiple input files\n");
        return -1;
    }
    if (redirect_output > 1) {
        fprintf(stderr, "syntax error: multiple output files\n");
        return -1;
    }
    return 0;
}

/*
 * executes commands, handles redirecting input/ output, and manages progresses
 */
int execute(char *arg[1024], char *redirect[1024]) {
    /* handle built-in shell commands */
    if (!strcmp(arg[0], "cd")) {
        if (arg[1] == NULL) {  //"cd" not followed by a directory name
            fprintf(stderr, "cd: syntax error\n");
            return -1;
        } else if (chdir(arg[1]) == -1) {
            perror("cd");
            return -1;
        }
        return 1;
    } else if (!strcmp(arg[0], "ln")) {
        // if "ln" not followed by source and destination names
        if (arg[1] == NULL || arg[2] == NULL) {
            fprintf(stderr, "ln: syntax error\n");
            return -1;
        } else if (link(arg[1], arg[2]) == -1) {
            perror("ln");
            return -1;
        }
        return 1;
    } else if (!strcmp(arg[0], "rm")) {
        if (arg[1] == NULL) {  // if "rm" not followed by a directory name
            fprintf(stderr, "rm: syntax error\n");
            return -1;
        } else if (unlink(arg[1]) == -1) {
            perror("rm");
            return -1;
        }
        return 1;
    } else if (!strcmp(arg[0], "exit")) {
        cleanup_job_list(job_list);
        exit(0);

        /* handle "jobs", "bg", and "fg" */
    } else if (!strcmp(arg[0], "jobs")) {
        if (arg[1] != NULL) {  // if "jobs" is followed by other inputs
            fprintf(stderr, "jobs: syntax error\n");
            return -1;
        } else {
            jobs(job_list);
            return 1;
        }
    } else if (!strcmp(arg[0], "bg")) {
        if (arg[1] == NULL) {  // if "bg" not followed by a jid
            fprintf(stderr, "bg: syntax error\n");
            return -1;
        } else {
            if (arg[1][0] != '%') {  // if the jid doesn't start with %
                fprintf(stderr, "bg: job input does not begin with %% \n");
                return -1;
            } else {
                char *percent = strchr(arg[1], '%');
                percent++;
                arg[1] = percent;  // removing the first char %
                int jid;
                if ((jid = atoi(arg[1])) == 0) {  // invalid jid
                    fprintf(stderr, "job not found \n");
                    return -1;
                } else {
                    int pid;
                    if ((pid = get_job_pid(job_list, jid)) <
                        0) {  // convert jid to pid
                        fprintf(stderr, "job not found \n");
                        return -1;
                    } else {
                        if (kill(-pid, SIGCONT)) {  // send the continue signal
                            perror("kill");
                            return -1;
                        }
                    }
                }
            }
        }
    } else if (!strcmp(arg[0], "fg")) {
        if (arg[1] == NULL) {  //"bg" not followed by pid
            fprintf(stderr, "fg: syntax error\n");
            return -1;
        } else {
            if (arg[1][0] != '%') {
                fprintf(stderr, "fg: job input does not begin with %% \n");
            } else {
                char *percent = strchr(arg[1], '%');
                percent++;
                arg[1] = percent;  // remove the first char %
                int jid;
                if ((jid = atoi(arg[1])) == 0) {          // convernt str to int
                    fprintf(stderr, "job not found \n");  // invalid jid
                    return -1;
                }
                int pid;
                if ((pid = get_job_pid(job_list, jid)) <
                    0) {  // convert jid to pid
                    fprintf(stderr, "job not found \n");
                    return -1;
                } else {
                    tcsetpgrp(0, pid);  // give the foreground process the
                                        // terminal control
                    if (kill(-pid,
                             SIGCONT)) {  // send the continue signal to all
                                          // processes in the process group
                        perror("kill");
                        return -1;
                    }
                    int wret, wstatus;
                    wret = waitpid(
                        pid, &wstatus,
                        WUNTRACED);  // wait for the foreground process to end
                    tcsetpgrp(0,
                              getpid());  // give terminal control back to shell
                    if (WIFEXITED(wstatus)) {
                        if (remove_job_pid(job_list, wret)) {
                            fprintf(stderr, "remove_job_pid error\n");
                            cleanup_job_list(job_list);
                            exit(1);
                        }
                    }
                    if (WIFSIGNALED(
                            wstatus)) {  // process terminated by a signal.
                        if (printf("[%d] (%d) terminated by signal %d\n", jid,
                                   wret, WTERMSIG(wstatus)) < 0) {
                            fprintf(stderr, "printf error\n");
                            return -1;
                        }
                        remove_job_pid(job_list, wret);
                    }

                    if (WIFSTOPPED(wstatus)) {  // process stopped by a signal,
                        update_job_pid(job_list, pid,
                                       STOPPED);  // update the state
                        if (printf("[%d] (%d) suspended by signal %d\n", jid,
                                   wret, WSTOPSIG(wstatus)) < 0) {
                            fprintf(stderr, "printf error\n");
                            return -1;
                        }
                    }
                }
            }
        }
    }

    /* fork into a child process */
    else {
        pid_t pid;
        if ((pid = fork()) == 0) {
            setpgid(0,
                    0);  // set the process group ID of a job to its process ID
            if (!is_background) {
                pid_t pgrp = getpgrp();
                tcsetpgrp(
                    0, pgrp);  // give the foreground process terminal control
            }

            if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
                perror("signal error");
                cleanup_job_list(job_list);
                exit(1);
            }
            if (signal(SIGTSTP, SIG_DFL) == SIG_ERR) {
                perror("signal error");
                cleanup_job_list(job_list);
                exit(1);
            }
            if (signal(SIGTTOU, SIG_DFL) == SIG_ERR) {
                perror("signal error");
                cleanup_job_list(job_list);
                exit(1);
            }

            /* handle redirection */
            if (redirect[0] != NULL) {
                // handle the error of consecutive redirect symbols
                if (!(strcmp(redirect[1], "<") && strcmp(redirect[1], ">") &&
                      strcmp(redirect[1], ">>"))) {
                    fprintf(
                        stderr,
                        "syntax error: output file is a redirection symbol\n");
                    cleanup_job_list(job_list);
                    exit(1);
                } else {
                    // iterate twice because the redirect array has four
                    // elements max
                    for (int i = 0; i < 4; i += 2) {
                        if (redirect[i] != NULL) {
                            if (!(strcmp(redirect[i], ">"))) {
                                close(1);  // close the stand output descriptor
                                if ((open(redirect[i + 1],
                                          O_CREAT | O_TRUNC | O_WRONLY,
                                          0666)) == -1) {
                                    perror("open");
                                    cleanup_job_list(job_list);
                                    exit(1);
                                }
                            } else if (!(strcmp(redirect[i], ">>"))) {
                                close(1);  // close the stand output descriptor
                                if ((open(redirect[i + 1],
                                          O_CREAT | O_APPEND | O_WRONLY,
                                          0666)) == -1) {
                                    perror("open");
                                    exit(1);
                                }
                            } else if (!(strcmp(redirect[i], "<"))) {
                                close(0);  // close the stand input descriptor
                                if ((open(redirect[i + 1], O_RDONLY, 0666)) ==
                                    -1) {
                                    perror("open");
                                    cleanup_job_list(job_list);
                                    exit(1);
                                }
                            }
                        }
                    }
                }
            }

            if (arg[0] != NULL) {
                char *full_path = arg[0];
                // process the full path to binary name
                char *backslash = strchr(arg[0], '/');
                if (backslash != NULL) {  // if there's blackslash in argv[0]
                    char *last_backslash = strrchr(arg[0], '/');
                    if (last_backslash != NULL) {
                        last_backslash++;
                        arg[0] = last_backslash;
                    } else {
                        arg[0] = "";
                    }  // if the file_path is ill-formatted
                }
                // execute the file
                if (execv(full_path, arg) == -1) {
                    perror("execv");
                    cleanup_job_list(job_list);
                    exit(1);
                }
            }
        }

        if (pid == -1) {  // if fork produces an error
            perror("fork");
            return -1;
        }

        if (is_background) {  // for background procces
            add_job(job_list, current_jid, pid, RUNNING, arg[0]);
            if (printf("[%d] (%d)\n", current_jid, pid) < 0) {
                /* handle a write error */
                fprintf(stderr, "printf error\n");
                cleanup_job_list(job_list);
                return -1;
            }
            current_jid++;
        }

        if (!is_background) {  // for foreground processes
            int wret, wstatus;
            wret = waitpid(pid, &wstatus, WUNTRACED);
            tcsetpgrp(0, getpid());  // give the control back to shell

            if (WIFSIGNALED(wstatus)) {  // process terminated by a signal.
                if (printf("[%d] (%d) terminated by signal %d\n", current_jid,
                           wret, WTERMSIG(wstatus)) < 0) {
                    /* handle a write error */
                    fprintf(stderr, "printf error\n");
                    return -1;
                }
            }

            if (WIFSTOPPED(wstatus)) {  // process stopped by a signal,
                add_job(job_list, current_jid, wret, STOPPED, arg[0]);
                if (printf("[%d] (%d) suspended by signal %d\n", current_jid,
                           wret, WSTOPSIG(wstatus)) < 0) {
                    /* handle a write error */
                    fprintf(stderr, "printf error\n");
                    return -1;
                }
                current_jid++;
            }
        }
    }

    return 0;
}

int main() {
    job_list = init_job_list();
    /* ignore the following signals when there isn't a foreground process */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("signal");
        cleanup_job_list(job_list);
        exit(1);
    }
    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
        perror("signal");
    }
    if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
        perror("signal");
        cleanup_job_list(job_list);
        exit(1);
    }

    while (1) {
        memset(buffer, 0, 1024);
        memset(tokens, 0, 512);
        memset(arg, 0, 512);
        memset(redirect, 0, 512);

#ifdef PROMPT
        if (printf("33sh> ") < 0) {
            /* handle a write error */
            fprintf(stderr, "Error writing the prompt\n");
            exit(1);
        }
        if (fflush(stdout) < 0) {
            /* handle error*/
            perror("flush");
        }
#endif
        ssize_t read_return = read(0, buffer, 1024);
        if (read_return > 0) {
            buffer[read_return] = '\0';  // terminate the buffer with null
        } else if (read_return == 0 || read_return == EOF) {
            cleanup_job_list(job_list);
            exit(0);
        } else {
            perror("read");
            cleanup_job_list(job_list);
            exit(1);
        }

        if (parse(buffer, tokens, arg, redirect) <
            0) {  // if an error occurred in parse
            continue;
        }

        if (arg[0] != NULL) {
            if (execute(arg, redirect) <
                0) {  // if an error occurred in execute
                continue;
            }
        } else if (redirect[0] != NULL) {
            fprintf(stderr, "error: redirects with no command\n");
            continue;
        }

        int wret, wstatus;
        while ((wret = waitpid(-1, &wstatus,
                               WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
            // examine all children who’ve terminated, stopped, or continued
            int jid = get_job_jid(job_list, wret);
            if (WIFEXITED(wstatus)) {
                if (printf("[%d] (%d) terminated with exit status %d\n", jid,
                           wret, WEXITSTATUS(wstatus)) < 0) {
                    /* handle a write error */
                    fprintf(stderr, "printf error\n");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                if (remove_job_pid(job_list, wret)) {
                    fprintf(stderr, "remove_job_pid error\n");
                    cleanup_job_list(job_list);
                    exit(1);
                }
            }

            if (WIFSIGNALED(wstatus)) {  // terminated by a signal .
                if (printf("[%d] (%d) terminated by signal %d\n", jid, getpid(),
                           WTERMSIG(wstatus)) < 0) {
                    /* handle a write error */
                    fprintf(stderr, "printf error\n");
                    cleanup_job_list(job_list);
                    exit(1);
                }
                if (remove_job_pid(job_list, wret)) {
                    fprintf(stderr, "remove_job_pid error\n");
                    cleanup_job_list(job_list);
                    exit(1);
                }
            }

            if (WIFSTOPPED(wstatus)) {  // process stopped by a signal,
                update_job_pid(job_list, wret, STOPPED);
                if (printf("[%d] (%d) suspended by signal %d\n", jid, getpid(),
                           WSTOPSIG(wstatus)) < 0) {
                    /* handle a write error */
                    fprintf(stderr, "printf error\n");
                    cleanup_job_list(job_list);
                    exit(1);
                }
            }

            if (WIFCONTINUED(wstatus)) {  // process resumed by SIGCONT
                // update the job
                update_job_pid(job_list, wret, RUNNING);

                int jid = get_job_jid(job_list, wret);
                if (printf("[%d] (%d) resumed\n", jid, wret) < 0) {
                    /* handle a write error */
                    fprintf(stderr, "printf error\n");
                    cleanup_job_list(job_list);
                    exit(1);
                }
            }
        }
    }
    cleanup_job_list(job_list);
    exit(0);
}
