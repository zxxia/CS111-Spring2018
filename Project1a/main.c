#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <errno.h>

#define BUFFER_SIZE 256
int running = 1;
char buffer[BUFFER_SIZE];
static int shell = 0;
struct termios orig_term_attrs;

void change_terminal_settings() {
    int rv;
    struct termios term_attrs;
    rv = tcgetattr(STDIN_FILENO, orig_term_attrs);
    if (rv == -1) {
        fprintf(stderr, "tcgetattr: %s\n.", strerror(errno));
        exit(1);
    }

    // Copy the original settings to a new struct
    memcpy(&term_attrs, orig_term_attrs, sizeof(term_attrs));
    // Change terminal attribute settings
    term_attrs.c_iflag = ISTRIP; // only lower 7 bits
    term_attrs.c_oflag = 0; // no processing
    term_attrs.c_lflag = 0; // no processing

    // Apply chanaged termianl settings
    rv = tcsetattr(STDIN_FILENO, TCSANOW, &term_attrs);
    if (rv == -1) {
        fprintf(stderr, "tcsetattr: %s\n.", strerror(errno));
        exit(1);
    }
}

void restore_terminal_settings() {
    // Reset termianl back to original settings
    int rv;
    rv = tcsetattr(STDIN_FILENO, TCSANOW, orig_term_attrs);
    if (rv == -1) {
        fprintf(stderr, "tcsetattr: %s\n.", strerror(errno));
        exit(1);
    }
}

void error_handler(char syscall_name[]) {
    fprintf(stderr, "%s: %s\n.", syscall_name, strerror(errno));
    restore_terminal_settings(orig_term_attrs);
    exit(1);
}

void child_process() {
    // In child process
    int rv = execl("/bin/bash", "/bin/bash", (char*) NULL);
    if (rv == -1) {
        error_handler("execl");
    }
}

void write_char_to_fd(int fd, char ch) {
    int bytes_wrtn = write(fd, &ch, 1);
    if (bytes_wrtn == -1) {
        error_handler("write");
    }
}

void transfer_char(int p2c_pipefd_wr, int c2p_pipefd_rd) {
    int rv;
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN | POLLHUP | POLLERR;

    fds[1].fd = c2p_pipefd_rd;
    fds[1].events = POLLIN | POLLHUP | POLLERR;

    while(running) {
        rv = poll(fds, 2, 0);
        if (rv == -1) { // An error occured when calling poll
            fprintf(stderr, "poll: %s\n.", strerror(errno));
            exit(1);
        } else if (rv == 0){ // Timeout and no file descriptor is ready
            continue;
        }

        // At least one file descriptor is ready
        for (int i = 0; i < 2; i++) {
            if (fds[i].revents & POLLIN) { // Data is ready at fd
                // Read the data into buffer
                int bytes_rd = read(fds[i].fd, buffer, BUFFER_SIZE);
                if (bytes_rd == -1) {
                    fprintf(stderr, "read: %s\n.", strerror(errno));
                    exit(1);
                }
                
                // Check all characters read
                for (int j = 0; j < bytes_rd; j++) {
                    if (buffer[j] == 0x04 && i == 0) {
                        // EOT from keyboard
                        close(p2c_pipefd_wr);
                        break;
                    } else if (buffer[j] == 0x0D || buffer[j] == 0x0A) {
                        // Newline from either keyboard or shell
                        
                        // Map newline from either keyboard or shell to stdout
                        char newln[2] = {0x0D, 0x0A};
                        write_char_to_fd(STDOUT_FILENO, newln[0]);
                        write_char_to_fd(STDOUT_FILENO, newln[1]);

                        if (i == 0) {
                            // Map newline from keyboard to shell
                            write_char_to_fd(p2c_pipefd_wr, newln[1]);
                        }
                    } else {
                        // Map char from either keyboard or shell to stdout
                        write_char_to_fd(STDOUT_FILENO, buffer[j]);
                        if (i == 0) {
                            // Map char from keyboard to shell
                            write_char_to_fd(p2c_pipefd_wr, buffer[j]);
                        }
                    }
                }
            }
        }

        if (fds[1].revents & (POLLHUP | POLLERR) && !(fds[1].revents & POLLIN)) {
            // The shell already closed the pipe.
            fprintf(stderr, "Shell closed the pipe./n");
            running = 0;
        }   
    }
}

int main(int argc, char *argv[])
{
    int rv;
    pid_t cpid;
    while(1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"shell", no_argument, &shell, 1},
            {0, 0, 0, 0}
        };

        rv = getopt_long(argc, argv, "", long_options, &option_index);
        if (rv == -1) {
            break;
        }

        switch (rv) {
            case '?':
                fprintf(stderr, "getopt: %s\n", strerror(errno));
                exit(1);
                break;
            case 0:
                break;
            default:
                break;
        }
    }

    change_terminal_settings();

    if(shell) {
        int p2c_pipefd[2];
        int c2p_pipefd[2];

        rv = pipe(p2c_pipefd);
        if (rv == -1) {
            fprintf(stderr, "pipe: %s\n.", strerror(errno));
            exit(1);
        }
        rv = pipe(c2p_pipefd);
        if (rv == -1) {
            fprintf(stderr, "pipe: %s\n.", strerror(errno));
            exit(1);
        }

        cpid = fork();
        if(cpid == -1) {
            fprintf(stderr, "fork: %s\n.", strerror(errno));
            exit(1);
        } else if (cpid == 0) {
            // Child Process
            close(c2p_pipefd[0]); // Close unused read end at c2p pipe
            close(p2c_pipefd[1]); // Close unused write end at p2c pipe
            
            close(STDIN_FILENO); // Transfer stdin to read end of p2c pipe
            dup(p2c_pipefd[0]);
            close(p2c_pipefd[0]);

            close(STDOUT_FILENO); // Transfer stdout to write end of c2p pipe
            dup(c2p_pipefd[1]);
            close(c2p_pipefd[1]);


            child_process();
        } else {
            // In Parent process
            close(p2c_pipefd[0]); // Close unused read end at out pipe
            close(c2p_pipefd[1]); // Close unused write end at in pipe
            transfer_char(p2c_pipefd[1], c2p_pipefd[0]);
            int exit_status;
            rv = waitpid(cpid, &exit_status, 0);
            fprintf(stdout,
                    "SHELL EXIT SIGNAL=%d STATUS=%d\n",
                    WTERMSIG(exit_status),
                    WEXITSTATUS(exit_status));
        }   
    } else { // No need to run shell
        //process_char();
    }

    restore_terminal_settings(&orig_term_attrs);

    exit(0);
}
