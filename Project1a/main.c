#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define BUFFER_SIZE 128

int main(int argc, char *argv[])
{
    int rv;
    struct termios orig_term_attrs;
    struct termios term_attrs;
    char buffer[BUFFER_SIZE];
    int running = 1;
    static int shell = 0;
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
        int child_pid = fork();
        if(child_pid == -1) {
            //TODO: handle error exception
            exit(1);
        } else if (child == 0) {
            // In child process
        } else {
            // In Parent process
        }   
    }
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
            } else {
                int bytes_wrtn = write(STDOUT_FILENO, buffer+i, 1);
                if (bytes_wrtn == -1) {
                    //TODO: handle error exception
                    exit(1);
                }
            }
        }
    }

    // Reset termianl back to original settings
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_attrs);
    if (rv == -1) {
        //TODO: handle error exception
        exit(1);
    }

    exit(0);
}
