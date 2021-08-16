/*
 * client.c
 * Carter King
 * CS 330 Operating Systems
 * Dr. Larkins
 * Due February 13th, 2019
 *
 * This program is the client of a chat server. It takes in a portal number
 * to connect to the server connected to the other side of the socket. From 
 * STDIN and STDOUT, chats are sent back and forth between the server and 
 * client
 *
 * Sites used:
 * https://www.gnu.org/software/libc/manual/html_node/Inet-Example.html
 * https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.15/gtpc2/cpp_gethostbyname.html
 * https://gist.github.com/listnukira/4045628
 * https://beej.us/guide/bgnet/html/multi/syscalls.html#gethostname
 *
 */


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netdb.h>

int main(int argc, char **argv) {
  int opt, portNum, reuseCheck, sockFID, portNetBytes, connCheck, bytesRead, stdBytesRead, bytesWritten;
  int val = 1;
  char buf[1024];
  struct sockaddr_in sockAddr;
  struct hostent* host;
  unsigned int sockLen = sizeof(sockAddr);
  char hostName[128];

  if(argc > 3){
    perror("argument error: ");
    exit(1);
  }
  
  //allow stdout to print regardless of a buffer
  setbuf(stdout, NULL);
  
  //Intake of command line arguments
  while((opt = getopt(argc, argv, "hp:")) != -1){
    switch(opt){
      case 'h':
        printf("usage: ./server [-h] [-p port #] \n -h - this help message \n -p # - the port to use when connecting to the server");
        break;
      case 'p':
        portNum = atoi(optarg);
        break;
      default:
        printf("Ya messed up your parameters \n");
        exit(1);
    }
  }


  //create a TCP socket
  sockFID = socket(AF_INET, SOCK_STREAM, 0);
  if(sockFID == -1){
    perror("Messed up creating client socket:");
    exit(1);
  }
  
  host = gethostbyname("localhost");
  if(host == NULL){
    perror("getting host name:");
    exit(1);
  }
  // port into network byte order
   portNetBytes = htons(portNum);
  // set the sockets port to the given one
  sockAddr.sin_port = portNetBytes;
  //set socket type to AF_INET
  sockAddr.sin_family = AF_INET;
  //connect the socket to the server
  memcpy(&sockAddr.sin_addr.s_addr, host->h_addr, host->h_length);

  //allow port connected to sockket to be re-used in quick concession
  reuseCheck = setsockopt(sockFID, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
  if(reuseCheck == -1){
    perror("setsockopt: ");
    exit(1);
  }
  
  //connect socket to proper address on server side
  connCheck = connect(sockFID, (struct sockaddr *) &sockAddr, sockLen);
  if(connCheck == -1){
    perror("connect: ");
    exit(1);
  }

  //connection success and prompt user
  fprintf(stdout, "Connected to Server...\n>");
 
  // initially read from STDIN
  bytesRead = read(STDIN_FILENO, buf, sizeof(buf));
  if(bytesRead == -1){
    perror("reading from monitor: ");
    exit(1);
  }
  //if client user immediately ^D
  if(bytesRead == 0){
    fprintf(stdout, "Hanging up ... \n");
  }

  //while you can still read from STDIN
  while(bytesRead > 0){
    //write what was read from STDIN to Server
    bytesWritten = write(sockFID, buf, bytesRead);
    if(bytesWritten == -1){
      perror("writing form server: ");
      exit(1);
    }
    //read from socket
    stdBytesRead = read(sockFID, buf, sizeof(buf));
    if(stdBytesRead == -1){
      perror("reading from STDIN: ");
      exit(1);
    }
    //check if server sent ^D
    if(stdBytesRead == 0){
      fprintf(stdout, "server hung up \n");
      break;
    }

    fprintf(stdout, "\n");
    //write from socket to STDOUT
    write(STDOUT_FILENO, buf, stdBytesRead);
    //prompt user to reply
    fprintf(stdout, ">");
    //continue reading from Server
    bytesRead = read(STDIN_FILENO, buf, sizeof(buf));
    if (bytesRead == -1){
      perror("STDIN read: ");
      exit(1);
    }
    //check if client user ^D after first beyond first input
    if (bytesRead == 0){
      fprintf(stdout, "hanging up ... \n");
      break;
    }
  }
  close(sockFID);
  return 0;
}

