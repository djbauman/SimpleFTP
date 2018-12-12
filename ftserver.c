/*
Daniel Bauman
(ftserver.c)
Program Description: This program listens on a user-determined port for a client connection.
Upon receiving a specific command from a client on this port, it uses another port to send 
either a list of the files in its directory, or a specific file requested by the client.

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>

#define MAX_MSG 501 	// max number of bytes we can get at once
#define BACKLOG 10  	// how many pending connections queue will hold

void catchSIGINT(int signo){
		printf("\nCtrl+C received; exiting server.\n");
		exit(0);
}

void sigchld_handler(int s)
{
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

// Function to get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
		if (sa->sa_family == AF_INET) {
				return &(((struct sockaddr_in*)sa)->sin_addr);
		}
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Function to listen on a given port
int establishConnection(int portNum, int* sockfd){
  struct addrinfo hints, *servinfo, *p;
  struct sigaction sa;
  int yes=1;
  int rv;
  char port[10]; 
  sprintf(port, "%d", portNum); // convert portNum to string

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(*sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(*sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
  int sockfd_control, sockfd_data = 0;	// socket file descriptors
  socklen_t sin_size;
  struct sockaddr_storage their_addr; 	// connector's address information
  char s[INET6_ADDRSTRLEN];

  int control_fd, data_fd;							// File descriptors for control & data sockets
  int numbytes;													// Holds return value for send() function
  int controlPort = atoi(argv[1]);			// Port number for Control Connection
  int dataPort = NULL;									// Port number for Data Connection (get from client)
  int dataPortPrev = 0;									// Stores previous Data Port #
	char msgIn[MAX_MSG];									// Buffer to receive messages
	char msgOut[MAX_MSG];									// Buffer to send messages
	char fileName[MAX_MSG];								// Buffer to hold filename (get from client)
	char command[2];											// Buffer to hold -l or -g command (set by client)
	int fileFound = 0;										// Flag to be set when requested file is found

	// Struct for catching SIGINT
	struct sigaction SIGINT_action = {0}, sigStopSettings = {0};
	SIGINT_action.sa_handler = catchSIGINT;         // Set handler function
	sigfillset(&SIGINT_action.sa_mask);             // All signals are blocked while sa_handler executes
	SIGINT_action.sa_flags = SA_RESTART;            // Set no flags
	sigaction(SIGINT, &SIGINT_action, NULL);

	if (argc != 2) {
		fprintf(stderr,"To run, enter \"server [port #]\"\n");
		exit(1);
	}

	// Establish Control Connection
	establishConnection(controlPort, &sockfd_control);

	// Listen for client (begin while loop)
	while(1) {
		printf("Listening for client connection...\n");
		sin_size = sizeof their_addr;
		control_fd = accept(sockfd_control, (struct sockaddr *)&their_addr, &sin_size);
		if (control_fd == -1) {
			perror("accept"); exit(1); continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		// printf("server: got connection from %s\n", s);
		
		// Receive message from client
    numbytes = recv(control_fd, msgIn, MAX_MSG-1, 0);
		msgIn[numbytes] = '\0';
		strcpy(fileName, msgIn);
		// printf("Received message from client: %s\n", msgIn);

		// If msgIn was not "-l" or "-g", respond w/ error message
		if ((msgIn[1] != 'l') && (msgIn[1] != 'g')) {
			strcpy(msgOut, "Invalid command. Please use \"-l\" or \"-g <filename>\".");
      send(control_fd, msgOut, strlen(msgOut), 0);
		
		// Otherwise, send "OK" message
		} else {
			strcpy(msgOut, "OK");
			send(control_fd, msgOut, strlen(msgOut), 0);

			// Set 'command' variable
			if (msgIn[1] == 'l') {
				strcpy(command, "-l");
			} else {
				strcpy(command, "-g");
			}

			// Get dataPort value from client...
			numbytes = recv(control_fd, msgIn, MAX_MSG-1, 0);
			msgIn[numbytes] = '\0';
			
			// If first time through loop, dataPortPrev gets msgIn
			if (dataPort == NULL) {
				dataPortPrev = atoi(msgIn);
				// Otherwise, dataPortPrev gets the last dataPort
			} else {
				dataPortPrev = dataPort;
			}
			dataPort = atoi(msgIn);

			// ...and establish Data Connection (if first time through the loop OR dataPort is new)
			if ((sockfd_data == 0) || (dataPort != dataPortPrev)) {
				establishConnection(dataPort, &sockfd_data);
			}
			data_fd = accept(sockfd_data, (struct sockaddr *)&their_addr, &sin_size);

			// Open current directory
			struct dirent *currentFile;
			struct stat fileStats;
			DIR* dir = opendir(".");
			if (dir == NULL) {
				printf("Couldn't open directory.\n");
				exit(1);
			}

			// Loop over directory & append file names to string 'fileList'
			char fileList[MAX_MSG];
			memset(fileList, '\0', sizeof(fileList));
			while((currentFile = readdir(dir)) != NULL) {
				stat(currentFile->d_name, &fileStats);
				if (S_ISREG(fileStats.st_mode)){
					strcat(fileList, currentFile->d_name);
					strcat(fileList, "\n");
				}
			}
			closedir(dir);

			// If command was -l, send fileList over Data Connection
			if (command[1] == 'l') {
				strcpy(msgOut, fileList);
	      numbytes = send(data_fd, msgOut, strlen(msgOut), 0);

	      // Otherwise (if the command was -g <filename>), parse the requested file name
			} else {
				char* token;
				token = strtok(fileName, " ");
				token = strtok(NULL, " ");
				strcpy(fileName, token);

				// Loop over directory; if file is found, set fileFound flag
				dir = opendir(".");
				while((currentFile = readdir(dir)) != NULL) {
					if (strcmp(fileName, currentFile->d_name) == 0){
						fileFound = 1;
					}
				}
				closedir(dir);

				// If the file was not found, send "File not found" over Control Connection
				if (fileFound == 0) {
					strcpy(msgOut, "File not found");
					numbytes = send(control_fd, msgOut, strlen(msgOut), 0);

					// Otherwise, send "File found" over Control Connection...
				} else {
					strcpy(msgOut, "File found");
					numbytes = send(control_fd, msgOut, strlen(msgOut), 0);

					// ...then send the file via Data Connection
					FILE *fp;
					char buffer[MAX_MSG];
					fp = fopen (fileName, "r");
					while (fgets(buffer, sizeof(buffer), fp)) {
						numbytes = send(data_fd, buffer, strlen(buffer), 0);
					}
					fclose(fp);
					printf("Transfer complete.\n");

				}

			}
			
			close(data_fd);
			printf("Data connection closed (%d).\n", dataPort);

		}

	}

	close(control_fd);
	// printf("Control connection closed.\n");

	return 0;
}