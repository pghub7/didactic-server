#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"
#include "server.h"
#include "netbuffer.h"
#include "stdbool.h"
#include "unistd.h"
#include "stdlib.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#define MAX_LINE_LENGTH 1024
static void handle_client(int fd);
static void handle_client_commands(int fd);
static void cleanString(char *str,char *substr);
static int handleUserCommand(char *buf, char *expectedCommand);
int pasv_socket(int port);
int accept_pasv(int sockfd);

int sockfd;
int representationType; // if representationtype = 0, its in ascii type. if representationtype = 1, its in image type
int mode =0; // if mode = 1, supports stream mode
int stru =0; // if stru = 1, supports file structure type
bool pasv_mode = false;
static char rootdir[1024];

int main(int argc, char *argv[]) {

    // Check the command line arguments
    if (argc != 2) {
        usage(argv[0]);
        return -1;
    }


    // This is how to call the function in dir.c to get a listing of a directory.
    // It requires a file descriptor, so in your code you would pass in the file descriptor 
    // returned for the ftp server's data connection
    // printf("Printed %d directory entries\n", listFiles(1, "."));

    //get root directory
    getcwd(rootdir, sizeof(rootdir));
    printf("%s\r\n",rootdir);
    run_server(argv[1], handle_client);

    return 0;
}

/**
 * all our stuff to handle client side initial entry
 * @param fd - file descriptor
 */
void handle_client(int fd) {

    net_buffer_t recvBuf = nb_create(fd, MAX_LINE_LENGTH);
    char buf[MAX_LINE_LENGTH + 1] = {0};

    send_string(fd, "220 Please login with username 'cs317'.\r\n");

    bool end_connection = false;

    while (end_connection == false) {

        nb_read_line(recvBuf, buf);
        cleanString(buf,"\n");
        cleanString(buf,"\r");

        if (handleUserCommand(buf, "user") == 2) {
            char *command = strtok(buf, " ");
            char *arg = strtok(NULL, " ");
            if (strcasecmp(arg, "cs317") == 0) {
                send_string(fd, "230 Login successful.\r\n");
                handle_client_commands(fd);
                break;
            }
            else send_string(fd,"530 This FTP server is cs317 only.\r\n");
        }

        else if(handleUserCommand(buf, "quit") == 1) {
            send_string(fd,"221 Goodbye.\r\n");
            end_connection = true;
            close(fd);
        }

        else send_string(fd, "530 Not logged in.\r\n");

    }
}

/**
 * as the name suggests...
 * @param fd - file descriptor
 */
void handle_client_commands(int fd) {
    net_buffer_t recvbuf = nb_create(fd, MAX_LINE_LENGTH);
    char client_message[MAX_LINE_LENGTH + 1] = {0};


    while(nb_read_line(recvbuf,client_message) != -1) {    //end == false
        cleanString(client_message,"\n");
        cleanString(client_message,"\r");
        char cmessage[1024];
        strcpy(cmessage,client_message);


        if(strstr(client_message,"user") != NULL) {
            send_string(fd,"530 Already Logged in. Can't change from cs317 user.\r\n");
        }

        else if(handleUserCommand(cmessage, "quit") == 1) {
            send_string(fd,"221 Goodbye.\r\n");
            close(fd);
        }

        else if(handleUserCommand(cmessage, "cdup") == 1) {
            char path[1024];
            getcwd(path,sizeof(path));
            if(strcmp(rootdir,path) == 0) {
                send_string(fd,"550 Cannot change directory\r\n");
            }
            else {
                printf("while CDUP, intitial working directory:");
                printf("%s\r\n", path);
                char *p;
                p = path;
                size_t n = strlen(path);
                for (int i = n; i > 0; i--) {
                    if (p[i] == '/') {
                        p[i] = '\0';
                        break;
                    }
                }
                send_string(fd, "250 Okay.\r\n");
                chdir(path);
                char c[1024];
                printf("%s\r\n", getcwd(c, sizeof(c)));
            }
        }

        else if(handleUserCommand(cmessage, "cwd") == 2 ) {
            char *comm = strtok(cmessage," ");
            char *path = strtok(NULL, " ");
            if (strstr(path, ".") != 0 || strstr(path, "..") != 0)
                send_string(fd, "550 Cannot access file with ./ or ../.\r\n");
            else if (chdir(path) == 0) {
                send_string(fd, "250 Okay.\r\n");
                char dire[1024];
                printf("%s\n", getcwd(dire, sizeof(dire)));
            }
            else send_string(fd, "550 No such file or directory.\r\n");
        }

        else if(handleUserCommand(cmessage, "type") == 2) {
            char *comm = strtok(cmessage," ");
            char *arg = strtok(NULL," ");
            if(strcasecmp(arg,"A") == 0) {
                send_string(fd,"200 ASCII type set.\r\n");
                representationType = 0;
            }
            else if(strcasecmp(arg,"I") == 0) {
                send_string(fd,"200 Image type set.\r\n");
                representationType = 1;
            }
            else send_string(fd,"500 Invalid type. Only ASCII and Image type allowed.\r\n");
        }

        else if(handleUserCommand(cmessage, "stru") == 2) {
            char *comm = strtok(cmessage," ");
            char *arg = strtok(NULL," ");
            if(strcasecmp(arg,"F") == 0) {
                stru = 1;
                send_string(fd,"200 File structure type set.\r\n");
            }
            else send_string(fd,"504 Only supports file structure type.\r\n");

        }

        else if(handleUserCommand(cmessage, "mode") == 2) {
            char *comm = strtok(cmessage," ");
            char *arg = strtok(NULL," ");

            if(strcasecmp(arg,"S") == 0) {
                mode = 1;
                send_string(fd,"200 Stream mode set\r\n");
            }
            else send_string(fd,"504 Only supports stream mode.\r\n");
        }

        else if(handleUserCommand(cmessage, "NLST") == 1) {
            int newfd;
            if(pasv_mode == true) {
                newfd = accept_pasv(sockfd);
                if(newfd > 0) {
                    char dir[1024];
                    getcwd(dir, sizeof(dir));
                    send_string(fd, "150 Here comes the directory listing\r\n");
                    listFiles(newfd, dir);
                    send_string(fd, "226 transfer complete.\r\n");

                    close(newfd);
                    newfd = -1;
                    close(sockfd);
                    sockfd = -1;
                    pasv_mode = false;
                    memset(dir, 0, sizeof(dir));
                }
                else send_string(fd,"500 NLST failed.\r\n");
            }
            else send_string(fd,"500 PASV mode not set.\r\n");
        }

        else if(handleUserCommand(cmessage, "pasv") == 1) {
            // declaring  port number
            int p;
            char port[1024];

            //getting hostname
            char name[1024];
            size_t size = sizeof(name);
            gethostname(name,size);

            // translating hostname to ip
            struct hostent *ip = gethostbyname(name);
            char *address = inet_ntoa(*((struct in_addr *) ip->h_addr_list[0]));

            // extracting parts of ip address
            char buf[1024];
            strcpy(buf,address);
            char *ip1 = strtok(buf,".");
            int i1 = atoi(ip1);
            char *ip2 = strtok(NULL,".");
            int i2 = atoi(ip2);
            char *ip3 = strtok(NULL,".");
            int i3 = atoi(ip3);
            char *ip4 = strtok(NULL,".");
            int i4 = atoi(ip4);

            // generate random number
            do {
                srand(time(NULL));
                p = (rand()% 64512 + 1024);
                sprintf(port, "%d", p);
                printf("%s\r\n", port);
            } while((sockfd = pasv_socket(p)) < 0 );

            socklen_t inav2length = sizeof(struct sockaddr);
            struct sockaddr_in inav2;
            getsockname(sockfd,(struct sockaddr *)&inav2,&inav2length);
            p = ntohs(inav2.sin_port);
            printf("os assigned portno:%d\r\n",p);

            if(sockfd > 0) {
                pasv_mode = true;
                printf("before the troubling passive sring.\r\n");
                char pasvMesssage[55];
                sprintf(pasvMesssage,"227 Entering passive mode (%d,%d,%d,%d,%d,%d)\r\n",i1,i2,i3,i4,(p>>8)&0xff,p&0xff);
                send_all(fd,pasvMesssage,sizeof(pasvMesssage));
            }
            else send_string(fd,"500 Passive mode failed.\r\n");
        }

        else if(handleUserCommand(client_message, "RETR") == 2) {
            if(pasv_mode == true) {
                int newfd = accept_pasv(sockfd);
                if(fd > 0) {
                    send_string(fd,"150 Entering binary mode data connection.\r\n");
                    char *command = strtok(cmessage," ");
                    char *token = strtok(NULL," ");
                    printf("token:%s\r\n",token);
                    char currentdirectory[1024];
                    getcwd(currentdirectory,sizeof(currentdirectory));
                    if(token != NULL && (strstr(token,"/") != NULL)) {
                        char path[1024];
                        strcpy(path,token);
                        char *p;
                        p = path;
                        size_t n = strlen(path);
                        for (int i = n; i > 0; i--) {
                            if (p[i] == '/') {
                                p[i] = '\0';
                                break;
                            }
                        }
                        printf("path:%s\r\n",path);
                        chdir(path);
                        char c[1024];
                        printf("%s\r\n", getcwd(c, sizeof(c)));

                    }
                    char buff[1024];
                    char *a1 = strtok(client_message," ");
                    while(a1 != NULL) {
                        memset(buff,0,sizeof(buff));
                        strcpy(buff,a1);
                        a1 = strtok(NULL,"/");
                        printf("a1:%s\r\n",a1);
                        printf("buff:%s\r\n",buff);
                    }
                    printf("moshomoshi\r\n");
                    char *filename = buff;
                    printf("%s\r\n",filename);
                    FILE *filepointer = fopen(filename,"r");
                    chdir(currentdirectory);
                    printf("%d\r\n",filepointer);

                    if(filepointer == NULL)
                        send_string(fd,"451 File not found.\r\n");
                    else {
                        fseek(filepointer,0,SEEK_END);
                        int len = ftell(filepointer);
                        printf("len:%d\r\n",len);
                        char buf[len];
                        memset(buf,0,sizeof(buf));
                        size_t noOfElementsInFile;
                        fseek(filepointer,0,SEEK_SET);
                        noOfElementsInFile = fread(buf,sizeof(char),len,filepointer);
                        printf("no of noOfElementsInFile:%d\r\n",noOfElementsInFile);
                        //buf[noOfElementsInFile] = 0;
                        int s = send_all(newfd,buf,sizeof(buf));
                        if(s == -1) {
                            send_string(fd,"551 Couldn't read file properly.\r\n");
                        }

                        else send_string(fd,"226 Transfer complete.\r\n");
                    }

                    close(newfd);
                    newfd = -1;
                    close(sockfd);
                    sockfd = -1;
                    pasv_mode = false;
                }
                else send_string(fd,"425 Data connection no established.\r\n");
            }
            else send_string(fd,"425 PASV mode not set.\r\n");


        }
        else send_string(fd,"500 Invalid command.\r\n");
    }
}

/*
 * as the name suggests we just remove /n and /r to prettify
 */
void cleanString(char *str, char *substr) {
    int size = strlen(substr);
    char *a;
    while((a = strstr(str,substr))) {
        *a = '\0';
        strcat(str,a+size);
    }
}

/**
 * as the name suggests
 * @param buf - the command
 * @param expectedCommand - what we are comparing the command to
 * @return number of arguments in the client message
 */
int handleUserCommand(char *buf, char *expectedCommand) {

    char bufCopy[1024];
    strcpy(bufCopy,buf);
    int counter = 0;
    char *token = strtok(bufCopy," ");

    if(strcasecmp(token,expectedCommand) == 0) {
        while(token != NULL) {
            token = strtok(NULL," ");
            counter++;
        }
        return counter;
    }
    memset(bufCopy,0, sizeof(bufCopy));
}



