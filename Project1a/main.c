#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 256
int running = 1;
char buffer[BUFFER_SIZE];

void child_process(int in_pipefd[2], int out_pipefd[2]) {
    // In child process
    close(in_pipefd[0]); // Close unused read end at in pipe
    close(STDIN_FILENO);
    dup(in_pipefd[0]);
    close(in_pipefd[0]);

    close(out_pipefd[1]); // Close unused write end at out pipe
    close(STDOUT_FILENO);
    dup(out_pipefd[1]);
    close(out_pipefd[1]);


    int rv = execl("/bin/bash", "/bin/bash");
    if (rv == -1) {
        //TODO: handle error exception
        exit(1);
    }

    fprintf(stderr, "in child process");

}

void process_char(int shell, int in_pipefd[2], int out_pipefd[2]) {
    //Read keyboard characters
    while(running) {
        int bytes_rd = read(STDIN_FILENO, buffer, BUFFER_SIZE);
        if (bytes_rd == -1) {
            //TODO: handle error exception
            exit(1);
        }

        for (int i = 0; i < bytes_rd; i++) {
            if (buffer[i] == 0x04) { // break read loop when EOT
                running = 0;
                break;
            } else if (buffer[i] == 0x0D || buffer[i] == 0x0A) {
                // Mapping to new line
                char new_ln[2] = {0x0D, 0x0A};
                int bytes_wrtn = write(STDOUT_FILENO, new_ln, sizeof(new_ln));
                if (bytes_wrtn == -1) {
                    //TODO: handle error exception
                    exit(1);
                }

                if (shell) {
                    bytes_wrtn
                }
            } else {
                int bytes_wrtn = write(STDOUT_FILENO, buffer+i, 1);
                if (bytes_wrtn == -1) {
                    //TODO: handle error exception
                    exit(1);
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int rv;
    struct termios orig_term_attrs;
    struct termios term_attrs;
    static int shell = 0;
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
                fprintf(stdout, "Unrecognizable argument!\n");
                break;
            case 0:
                fprintf(stdout, "Recoginized shell argument %d!\n", shell);
                break;
            default:
                break;
        }
    }

    rv = tcgetattr(STDIN_FILENO, &orig_term_attrs);
    if (rv == -1) {
        //TODO: handle error exception
        exit(1);
    }

    // Copy the original settings to a new struct
    memcpy(&term_attrs, &orig_term_attrs, sizeof(orig_term_attrs));
    // Change terminal attribute settings
    term_attrs.c_iflag = ISTRIP; // only lower 7 bits
    term_attrs.c_oflag = 0; // no processing
    term_attrs.c_lflag = 0; // no processing

    // Apply chanaged termianl settings
    rv = tcsetattr(STDIN_FILENO, TCSANOW, &term_attrs);
    if (rv == -1) {
        //TODO: handle error exception
        exit(1);
    }

    if(shell) {
        int in_pipefd[2];
        int out_pipefd[2];

        rv = pipe(out_pipefd);
        if (rv == -1) {
            //TODO: pipe error handling
            exit(1);
        }
        rv = pipe(in_pipefd);
        if (rv == -1) {
            //TODO: pipe error handling
            exit(1);
        }

        cpid = fork();
        if(cpid == -1) {
            //TODO: handle fork error exception
            exit(1);
        } else if (cpid == 0) {
            fprintf(stderr, "in child process!\n");
            child_process(in_pipefd, out_pipefd);
        } else {
            // In Parent process
            close(out_pipefd[0]); // Close unused read end at out pipe
            close(in_pipefd[1]); // Close unused write end at in pipe
        }   
    } else { // No need to run shell
        process_char();
    }


    // Reset termianl back to original settings
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_attrs);
    if (rv == -1) {
        //TODO: handle error exception
        exit(1);
    }

    exit(0);
}
