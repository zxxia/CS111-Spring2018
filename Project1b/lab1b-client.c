#define _POSIX_SOURCE
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#define BUFFER_SIZE 256

int running = 1;
char buffer[BUFFER_SIZE];
static char* port = NULL;
static int log = 0;
static int compress = 0;
struct termios orig_term_attrs;

void parse();

void change_terminal_settings();

void restore_terminal_settings();

void error_handler(char syscall_name[]);

void child_process(); 

void write_char_to_fd(int fd, char ch); 

void comm(int sockfd); 


int port_check(char* port);

int main(int argc, char *argv[])
{
    int rv;
    char* hostname = "localhost";
    
    struct addrinfo hints, *servinfo, *p;

    change_terminal_settings();
    // make sure the struct is empty
    memset(&hints, 0, sizeof hints);
    // IPv4
    hints.ai_family = AF_INET;
    // TCP stream sockets
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "ERROR: getaddrinfo error: " << gai_strerror(rv);
        exit(1);
    }


    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("ERROR: client: socket");
            continue;
        }

        rv = fcntl(sockfd, F_SETFL, O_NONBLOCK);
        if(rv == -1){
    	    perror("ERROR: fcntl");
    	    continue;
        }

        rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
        if (rv == -1 && errno != EINPROGRESS) {
            close(sockfd);
            perror("ERROR: client: connect");
    	    continue;
        }
        break;
    }

    // All done with this result link list
    freeaddrinfo(servinfo);

    comm(sockfd);
    restore_terminal_settings(&orig_term_attrs);

    exit(0);
}


void parse(int argc, char* argv) {
    while(1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"port", required_argument, NULL, 1},
            {"log", no_argument, &log, 1},
            {"compress", no_argument,&compress, 1},
            {0, 0, 0, 0}
        };

        rv = getopt_long(argc, argv, "", long_options, &option_index);
        if (rv == -1) {
            break;
        }

        switch (rv) {
            case '?':
                restore_terminal_settings(); 
                exit(1);
            case 0:
                break;
            case 1:
                if (optarg && port_check(optarg)) {
                    printf("port=%s\n", optarg);
                    port = optarg;
                    break;
                } else {
                    fprintf(stderr, "Error: port number out of range.\n");
                    exit(1);
                }
            default:
                break;
        }
    }
}

int port_check(char* port) {
    return strtol(port, NULL, 0) >= 1024 && strtol(port, NULL, 0) <= 65535;
}


void change_terminal_settings() {
    int rv;
    struct termios term_attrs;
    rv = tcgetattr(STDIN_FILENO, &orig_term_attrs);
    if (rv == -1) {
        fprintf(stderr, "tcgetattr: %s\n.", strerror(errno));
        exit(1);
    }

    // Copy the original settings to a new struct
    memcpy(&term_attrs, &orig_term_attrs, sizeof(term_attrs));
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
    rv = tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_attrs);
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

void write_char_to_fd(int fd, char ch) {
    int bytes_wrtn = write(fd, &ch, 1);
    if (bytes_wrtn == -1) {
        error_handler("write");
    }
}

void comm(int sockfd) {
    int rv;
    int fd_num = shell + 1;

    struct pollfd fds[fd_num];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN | POLLHUP | POLLERR;
    if (shell) {
        fds[1].fd = c2p_pipefd_rd;
        fds[1].events = POLLIN | POLLHUP | POLLERR;
    }

    while(running) {
        rv = poll(fds, fd_num, 0);
        if (rv == -1) { // An error occured when calling poll
            error_handler("poll");
        } else if (rv == 0){ // Timeout and no file descriptor is ready
            continue;
        }

        // At least one file descriptor is ready
        for (int i = 0; i < fd_num; i++) {
            if (fds[i].revents & POLLIN) { // Data is ready at fd
                // Read the data into buffer
                int bytes_rd = read(fds[i].fd, buffer, BUFFER_SIZE);
                if (bytes_rd == -1) {
                    error_handler("read");
                }
                
                // Check all characters read
                for (int j = 0; j < bytes_rd; j++) {
                    if (buffer[j] == 0x04 && i == 0) {
                        // EOT from keyboard
                        // Wirte ^D to stdout
                        char ctrl_D[2] = {'^', 'D'};
                        write_char_to_fd(STDOUT_FILENO, ctrl_D[0]);
                        write_char_to_fd(STDOUT_FILENO, ctrl_D[1]);
                        if (shell) {
                            close(p2c_pipefd_wr);
                            break;
                        }
                        return;
                    } else if (buffer[j] == 0x03 && i== 0) {
                        // Write ^C to stdout
                        char ctrl_C[2] = {'^', 'C'};
                        write_char_to_fd(STDOUT_FILENO, ctrl_C[0]);
                        write_char_to_fd(STDOUT_FILENO, ctrl_C[1]);
                        // Kill shell
                        if (shell) {
                            rv = kill(cpid, SIGINT);
                            if (rv < 0) {
                                error_handler("kill");
                            }
                        }
                    } else if (buffer[j] == 0x0D || buffer[j] == 0x0A) {
                        // Newline from either keyboard or shell
                        
                        // Map newline from either keyboard or shell to stdout
                        char newln[2] = {0x0D, 0x0A};
                        write_char_to_fd(STDOUT_FILENO, newln[0]);
                        write_char_to_fd(STDOUT_FILENO, newln[1]);

                        if (shell && i == 0) {
                            // Map newline from keyboard to shell
                            write_char_to_fd(p2c_pipefd_wr, newln[1]);
                        }
                    } else {
                        // Map char from either keyboard or shell to stdout
                        write_char_to_fd(STDOUT_FILENO, buffer[j]);
                        if (shell && i == 0) {
                            // Map char from keyboard to shell
                            write_char_to_fd(p2c_pipefd_wr, buffer[j]);
                        }
                    }
                }
            }
        }

        if (shell && fds[1].revents & (POLLHUP | POLLERR)
            && !(fds[1].revents & POLLIN)) {
            // The shell already closed the pipe.
            running = 0;
        }
    }
}

