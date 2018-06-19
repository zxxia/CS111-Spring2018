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
#include <fcntl.h>
#define BUFFER_SIZE 256

int running = 1;
char buffer[BUFFER_SIZE];
static char* port = NULL;
static int log = 0;
static int compress = 0;
struct termios orig_term_attrs;

void parse(int argc, char *argv[]);

void change_terminal_settings();

void restore_terminal_settings();

void error_handler(char syscall_name[], int rv);

void child_process(); 

void write_char_to_fd(int fd, char ch); 

void comm(int sockfd); 

int port_check(char* port);

int set_conn();

void display_char(char ch);

int main(int argc, char *argv[])
{
    int sockfd; 
    change_terminal_settings();
    parse(argc, argv);
    sockfd = set_conn();
    comm(sockfd);
    restore_terminal_settings(&orig_term_attrs);
    exit(0);
}


void parse(int argc, char *argv[]) {
    int rv;
    while(1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"port", required_argument, NULL, 1},
            {"log", no_argument, &log, 1},
            {"compress", no_argument, &compress, 1},
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
                    error_handler("port number out of range.", rv);
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

void error_handler(char syscall_name[], int rv) {
    if (strcmp(syscall_name, "getaddrinfo") == 0) {
        fprintf(stderr, "%s: %s\n.", syscall_name, gai_strerror(rv));
    } else {
        fprintf(stderr, "%s: %s\n.", syscall_name, strerror(errno));
    }
    restore_terminal_settings(orig_term_attrs);
    exit(1);
}

void write_char_to_fd(int fd, char ch) {
    int bytes_wrtn = write(fd, &ch, 1);
    if (bytes_wrtn == -1) {
        error_handler("write", bytes_wrtn);
    }
}

void comm(int sockfd) {
    int rv;
    int fd_num = 2;

    struct pollfd fds[fd_num];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN | POLLHUP | POLLERR;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLHUP | POLLERR;


    while(running) {
        rv = poll(fds, fd_num, 0);
        if (rv == -1) { // An error occured when calling poll
            error_handler("poll", rv);
        } else if (rv == 0){ // Timeout and no file descriptor is ready
            continue;
        }

        // At least one file descriptor is ready
        for (int i = 0; i < fd_num; i++) {
            if (fds[i].revents & POLLIN) { // Data is ready at fd
                // Read the data into buffer
                int bytes_rd = read(fds[i].fd, buffer, BUFFER_SIZE);
                if (bytes_rd == -1) {
                    error_handler("read", rv);
                }

                // Check all characters read
                for (int j = 0; j < bytes_rd; j++) {
                    if (i == 0) { // Data from stdin
                        // Write char to sockfd
                        write_char_to_fd(sockfd, buffer[j]);    
                    }
                    display_char(buffer[j]);
                }
            }
        }

        /*if (fds[1].revents & (POLLHUP | POLLERR)
          && !(fds[1].revents & POLLIN)) {
        // The shell already closed the pipe.
        running = 0;
        }*/
    }
}


int set_conn() {
    int rv;
    int sockfd; 
    char* hostname = "localhost";
    struct addrinfo hints;
    struct addrinfo *servinfo, *rp;


    // make sure the struct is empty
    memset(&hints, 0, sizeof hints);
    // IPv4 or 6
    hints.ai_family = AF_UNSPEC;
    // TCP stream sockets
    hints.ai_socktype = SOCK_STREAM;
    rv = getaddrinfo(hostname, port, &hints, &servinfo);
    if (rv != 0) {
        error_handler("getaddrinfo", rv);
        exit(1);
    }


    // loop through all the results and connect to the first we can
    for(rp = servinfo; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            perror("ERROR: client: socket");
            continue;
        }

        /*rv = fcntl(sockfd, F_SETFL, O_NONBLOCK);
          if(rv == -1){
          perror("ERROR: fcntl");
          continue;
          }*/

        rv = connect(sockfd, rp->ai_addr, rp->ai_addrlen);
        if (rv == -1 && errno != EINPROGRESS) {
            close(sockfd);
            perror("ERROR: client: connect");
            continue;
        }
        break;
    }

    // All done with this result link list
    freeaddrinfo(servinfo);

    return sockfd;


}

void display_char(char ch){
    if (ch == 0x04) {
        // EOT from keyboard
        // Wirte ^D to stdout
        char ctrl_D[2] = {'^', 'D'};
        write_char_to_fd(STDOUT_FILENO, ctrl_D[0]);
        write_char_to_fd(STDOUT_FILENO, ctrl_D[1]);
    } else if (ch == 0x03) {
        // Write ^C to stdout
        char ctrl_C[2] = {'^', 'C'};
        write_char_to_fd(STDOUT_FILENO, ctrl_C[0]);
        write_char_to_fd(STDOUT_FILENO, ctrl_C[1]);
    } else if (ch == 0x0D || ch == 0x0A) {
        // Newline from either keyboard or shell

        // Map newline from either keyboard or shell to stdout
        char newln[2] = {0x0D, 0x0A};
        write_char_to_fd(STDOUT_FILENO, newln[0]);
        write_char_to_fd(STDOUT_FILENO, newln[1]);

    } else {
        // Map char from either keyboard or shell to stdout
        write_char_to_fd(STDOUT_FILENO, ch);
    }
}

