/*
 * server.c 
 * Carter King
 * CS 330 Operating Systems
 * Dr. Larkins
 * Due February 13th, 2019
 *
 * This porgram creates the server side of a chat client. This includes
 * a monitor aspect that is connected to a relay server by way of pipes.
 * The relay server uses a TCP socket connection to relay messages
 * between the server and the client
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


// constants for pipe FDs
#define WFD 1
#define RFD 0


/*
 * monitor() - provides a local chat window using pipes to read and write to the relay
 *  server as well as read and write to stdout and stdin.
 * @param srfd - server read file descriptor
 * @param swfd - server write file descriptor
 * returns: void
 */


void monitor(int srfd, int swfd) {
  int bytesRead, stdBytesRead, bytesWritten;
  char buf[1024];
  //allows printing to stdout regardless of a buffer
  setbuf(stdout, NULL);

  //Initially read from relay server
  bytesRead = read(srfd, buf, sizeof(buf));
  if(bytesRead == -1){
    perror("reading from monitor: ");
    exit(1);
  }

  //while you can still read from the relay server
  while(bytesRead > 0){
    //write what was read to STDOUT
    bytesWritten = write(STDOUT_FILENO, buf, bytesRead);
    if(bytesWritten == -1){
      perror("writing form server: ");
      exit(1);
    }

    fprintf(stdout, "\n");
    //print prompt to STDOUT
    fprintf(stdout, ">");
    //read from STDIN
    stdBytesRead = read(STDIN_FILENO, buf, sizeof(buf));
    if(stdBytesRead == -1){
      perror("reading from STDIN: ");
      exit(1);
    }
    //check for ^D from server user
    if(stdBytesRead == 0){
      fprintf(stdout, "hanging up on client ... \n");
      break;
    }
    //write from STDIN to relay server
    write(swfd, buf, stdBytesRead);
    //continue reading from relay server
    bytesRead = read(srfd, buf, sizeof(buf));
    if(bytesRead == -1){
      perror("reading from monitor 2: ");
      exit(1);
    }
    if(bytesRead == 0){
      break;
    }
  }
  return;
}


/*
 * server() - relays chat messages using pipes to the monitor, and reads and writes using
 *  sockets between the client program
 * @param mrfd - monitor read file descriptor
 * @param mwfd - monitor write file descriptor
 * @param portno - TCP port number to use for client connections
 * returns: void
 */


void server(int mrfd, int mwfd, int portno) {
  int portNetBytes, sockFID, lisCheck, setCheck, accSockFD, bytesMon, bytesClient;
  int listBacklog = 50;
  int val = 1;
  char buf[1024];
  struct sockaddr_in sockAddr, clientAddr;
  unsigned int sockSize = sizeof(sockAddr);
  unsigned int clientSize = sizeof(clientAddr);

  //Allows printing to stdout regardless of a buffer
  setbuf(stdout, NULL);

  //create a TCP socket
  sockFID = socket(AF_INET, SOCK_STREAM, 0);
  if(sockFID == -1){
    perror("socket()...");
    exit(1);
  }
  // port into network byte order
  portNetBytes = htons(portno);
  // set the sockets port to the given one
  sockAddr.sin_port = portNetBytes;
  //set the sockets address to any available interface
  sockAddr.sin_addr.s_addr = INADDR_ANY;
  //set socket family to Address Family
  sockAddr.sin_family = AF_INET;

  //allow the port to be re-used in quick concession
  setCheck = setsockopt(sockFID, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
  if(setCheck == -1){
    perror("setsockopt: ");
    exit(1);
  }

  //bind the newly created socket to an address in the new TCP port
  if((bind(sockFID,(struct sockaddr *) &sockAddr, sockSize)) == -1){
    perror("bind error: ");
    exit(1);
  }

  //listen for the client connection to socket
  lisCheck = listen(sockFID, listBacklog); 
  if(lisCheck == -1){
    perror("listen: ");
    exit(1);
  }

  //accept the client's requested connection
  accSockFD = accept(sockFID, (struct sockaddr *) &clientAddr, &clientSize);
  if (accSockFD == -1){
    perror("access: ");
    exit(1);
  }

  //read from the client
  bytesClient = read(accSockFD, buf, sizeof(buf));
  if(bytesClient == -1){
    perror("client Read: ");
    exit(1);
  }
  //check if client ^D without typing anything
  if(bytesClient == 0){
    fprintf(stdout, "client hung up ... \n");
  }

  //while you can read from the client
  while(bytesClient > 0){
    write(mwfd, buf, bytesClient);
    bytesMon = read(mrfd, buf, sizeof(buf));
    if(bytesMon == -1){
      perror("monitor read: ");
      exit(1);
    }
    else if(bytesMon == 0){
      break;
    }
    else{
      write(accSockFD, buf, bytesMon);
      bytesClient = read(accSockFD, buf, sizeof(buf));  
      if(bytesClient == -1){
        perror("reading from socket 2: ");
        exit(1);
      }
      //check if client sent over ^D
      if(bytesClient == 0){
        fprintf(stdout, "Client hung up ...\n");
        break;
      }
    }
  }
}



int main(int argc, char **argv) {
  // implement me
  int opt, portNum;
  pid_t pid;
  int M2SFds[2];
  int S2MFds[2];


  if(argc > 3){
    perror("argument error: ");
    exit(1);
  }

  //getopt() to help if a -h or -p # added to compile
  while((opt = getopt(argc, argv, "hp:")) != -1){
    switch(opt){
      case 'h':
        printf("usage: ./server [-h] [-p port #]\n"); 
        printf("%8s -h - this help message\n", " ");
        printf("%8s -p # - the port to use when connecting to the server\n", " ");
        exit(1);
      case 'p':
        portNum = atoi(optarg);
        break;
      default:
        printf("Ya messed up your parameters \n");
        exit(1);
    }

    // pipe from Monitor to Relay Server
    if(pipe(M2SFds) == -1){
      perror("Monitor to Server: ");
      exit(1);
    }
    
    //pipe from relay server to monitor
    if(pipe(S2MFds) == -1){
      perror("Server to Monitor: ");
      exit(1);
    }
    
    //fork into parent and child process
    pid = fork();
    if(pid == -1){
      perror("Fork Error ");
      exit(1);
    }
    else if(pid == 0){
      //child process as monitor function
      close(M2SFds[RFD]);
      close(S2MFds[WFD]);
      monitor(S2MFds[RFD], M2SFds[WFD]);
      close(M2SFds[WFD]);
      close(S2MFds[RFD]);
    }
    else{
      //parent proces as relay server
      close(S2MFds[RFD]);
      close(M2SFds[WFD]);
      server(M2SFds[RFD], S2MFds[WFD], portNum);
      close(M2SFds[RFD]);
      close(S2MFds[WFD]);
      pid = wait(NULL); 
    }
    return 0;
  }
}

