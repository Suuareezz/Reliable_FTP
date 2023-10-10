/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>

#define BUFSIZE (2048)	

struct vr_packet {
	long int id;
	long int pkt_len;
	char data[BUFSIZE];
};

static void error(const char *msg, ...)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

char* list_files() {
    struct dirent **dirent;
    int n = 0;

    if ((n = scandir(".", &dirent, NULL, alphasort)) < 0) {
        perror("Scanerror");
        return NULL;
    }

    size_t total_length = 0;
    for (int i = 0; i < n; ++i) {
        total_length += strlen(dirent[i]->d_name) + 1; // +1 for newline
    }

    char *result = (char *)malloc(total_length + 1); // +1 for null terminator

    if (result == NULL) {
        perror("Memory allocation error");
        return NULL;
    }

    char *current = result;
    for (int i = 0; i < n; ++i) {
        strcpy(current, dirent[i]->d_name);
        current += strlen(dirent[i]->d_name);
        *current++ = '\n';
        free(dirent[i]);
    }

    free(dirent);

    *current = '\0';
    return result;
}

int main(int argc, char **argv)
{

	if (argc != 2) {				
		printf("Usage: ./<%s> <Port Number(>5000)>\n", argv[0]);
		exit(1);
	}
    struct sockaddr_in serveraddr, clientaddr;
    char filename[40];         
	char cmd[40];
	struct vr_packet packet;
	struct timeval t_out = {0, 0};
	struct stat st;
    off_t f_size; 	
	int acknumb = 0, ack_send = 0;
	int sockfd;
    char message[BUFSIZE];
	ssize_t numRead;
	ssize_t length;

	FILE *fptr;

    memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(argv[1]));
	serveraddr.sin_addr.s_addr = INADDR_ANY;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		error("Server socket");

	if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) == -1)
		error("Server bind");

    while(1) {
		printf("Server Listening on port %d \n", atoi(argv[1]));

		memset(message, 0, sizeof(message));
		memset(cmd, 0, sizeof(cmd));
		memset(filename, 0, sizeof(filename));

		length = sizeof(clientaddr);

		if((numRead = recvfrom(sockfd, message, BUFSIZE, 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length)) == -1)
			error("Server receive");

		printf("Task to do: %s\n", message);

		sscanf(message, "%s %s", cmd, filename);
    
        if ((strcmp(cmd, "put") == 0) && (filename[0] != '\0')) {
            printf("Server: Put called with file name : %s\n", filename);

			long int num_of_packets = 0, bytes_rec = 0, i = 0;
			
			t_out.tv_sec = 2;
			setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval));   

			recvfrom(sockfd, &(num_of_packets), sizeof(num_of_packets), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);
			
			t_out.tv_sec = 0;
			setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval));
			
			if (num_of_packets > 0) {
				sendto(sockfd, &(num_of_packets), sizeof(num_of_packets), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
				printf("Total packet: %ld\n", num_of_packets);
	
				fptr = fopen(filename, "wb");

				for (i = 1; i <= num_of_packets; i++)
				{
					memset(&packet, 0, sizeof(packet));

					recvfrom(sockfd, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);  //Recieve the packet
                    printf("Replying ack:%ld", packet.id);
				    sendto(sockfd, &(packet.id), sizeof(packet.id), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));    //Send the ack
                    // code will wait to receive that packet again.
					if (packet.id !=i) {
						i--;
					}
					else {
                        bytes_rec += packet.pkt_len;  
						fwrite(packet.data, 1, packet.pkt_len, fptr);
						printf("packet.id: %ld	packet.length: %ld\n", packet.id, packet.pkt_len);
					}
					
					if (i == num_of_packets)
						printf("File recieved\n");
				}
                printf("bytes recieved: %ld\n", bytes_rec);
                fclose(fptr);
                sleep(5);
			}
			else {
				printf("File is empty\n");
			}
        }
        else if ((strcmp(cmd, "get") == 0) && (filename[0] != '\0')) {

			printf("get %s from server\n", filename);

			if (access(filename, F_OK) == 0) {	
				
				int num_of_packets = 0, resend_frame = 0;
				int i = 0, j = 0;
					
				stat(filename, &st);
				f_size = st.st_size;		

				t_out.tv_sec = 2;			
				t_out.tv_usec = 0;
				setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval));   //Set timeout option for recvfrom

				fptr = fopen(filename, "rb");       
					
				if ((f_size % BUFSIZE) != 0)
					num_of_packets = (f_size / BUFSIZE) + 1;
				else
					num_of_packets = (f_size / BUFSIZE);

				printf("number of packets: %d\n", num_of_packets);
					
				length = sizeof(clientaddr);

				sendto(sockfd, &(num_of_packets), sizeof(num_of_packets), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));	
				recvfrom(sockfd, &(acknumb), sizeof(acknumb), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);

				if (acknumb != num_of_packets)
				{
					for (j = 0; j < 25; j++) {
					sendto(sockfd, &(num_of_packets), sizeof(num_of_packets), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr)); 
					recvfrom(sockfd, &(acknumb), sizeof(acknumb), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);
                    if (acknumb == num_of_packets) {
                            break;
                        }
                    }
                    if (j == 25) {
                        printf("\n Error sending total number of packets to be sent\n");
                        break;
                    }
				}

				for (i = 1; i <= num_of_packets; i++)
				{
					memset(&packet, 0, sizeof(packet));
					acknumb = 0;
					packet.id = i;
					packet.pkt_len = fread(packet.data, 1, BUFSIZE, fptr);

					sendto(sockfd, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
					recvfrom(sockfd, &(acknumb), sizeof(acknumb), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);

					if (acknumb != packet.id)
					{
						for (j = 0; j < 50; j++) {
						// resend the packet and receive an ack again
						sendto(sockfd, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
						recvfrom(sockfd, &(acknumb), sizeof(acknumb), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);
						printf("packet %ld	dropped %d times\n", packet.id, j+1);
						
						if (acknumb == packet.id){
                                break;
                            }
                        }
					}
                    if (j == 50) {
                        printf("\n Error sending packet #%ld \n", packet.id);
                        break;
                    }

					printf("packet no: %d	ACK: %d \n", i, acknumb);

					if (num_of_packets == acknumb)
						printf("File sent\n");
    
				}
				fclose(fptr);
                t_out.tv_sec = 0;
				t_out.tv_usec = 0;
				setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval));
			}
			else {	
				printf("Invalid Filename\n");
			}
		}
        else if ((strcmp(cmd, "del") == 0) && (filename[0] != '\0')) {
            // Using delete function in C and send error status to client
			if(access(filename, F_OK) == -1) {
				ack_send = 1;
				sendto(sockfd, &(ack_send), sizeof(ack_send), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
			}
			else{
				if(access(filename, R_OK) == 2) { 
					ack_send = 0;
					sendto(sockfd, &(ack_send), sizeof(ack_send), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
				}
				else {
					printf("Filename is %s\n", filename);
					remove(filename);
					ack_send = 3;
					sendto(sockfd, &(ack_send), sizeof(ack_send), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
				}
			}
		} 
        else if (strcmp(cmd, "ls") == 0) {
			
			char *files = list_files();
			if (sendto(sockfd, files, strlen(files), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr)) == -1)
				error("Server: send");

		} 
        else if (strcmp(cmd, "exit") == 0) {
			close(sockfd);
			exit(0);
		}
        else {
			printf("Invalid Command\n");
		}
    }
}