/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 * Verified that both client and server side file has same size after send and receive. 
 */
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <stdarg.h>
#include <dirent.h>

#define BUFSIZE 2048

// Packet Structure
struct vr_packet {
	long int id;
	long int pkt_len;
	char data[BUFSIZE];
};

static void error(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

void Usage() {
  printf("Usage: ");
  printf("\n      ls");
  printf("\n      del <filename>");
  printf("\n      get <filename>");
  printf("\n      put <filename>");
  printf("\n      exit");
  
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Client: Usage: ./<%s> <ip> <port>\n", argv[0]);
		exit(1);
	}
	FILE *fptr;
    ssize_t delack = 0;
	off_t filesize = 0;
	long int acknumb = 0;
    ssize_t length = 0;
	int sockfd, ack_rx = 0;
    struct vr_packet packet;
	struct timeval t_out = {0, 0};
	char cmd_send[50], filename[50], cmd[50];
    struct sockaddr_in serveraddr, clientaddr;
	struct stat st;
	
	memset(&serveraddr, 0, sizeof(serveraddr));
	memset(&clientaddr, 0, sizeof(clientaddr));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(argv[2]));
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		error("Client: socket");
    
    while(1) {
		memset(cmd_send, 0, sizeof(cmd_send));
		memset(filename, 0, sizeof(filename));
        memset(cmd, 0, sizeof(cmd));
		
		Usage();
        printf("\nEnter:");
        fgets(cmd_send, 50, stdin);

		printf("Command: %s\n", cmd_send);
		
		sscanf(cmd_send, "%s %s", cmd, filename);

		if (sendto(sockfd, cmd_send, sizeof(cmd_send), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) == -1) {
			error("Client: send");
		}

        if ((strcmp(cmd, "put") == 0) && (filename[0] != '\0')) {
            if (access(filename, F_OK) == 0) {	
                int i = 0, j = 0, num_of_packets = 0;
				
				stat(filename, &st);
				filesize = st.st_size;	//Size of the file

				t_out.tv_sec = 2;
				t_out.tv_usec = 0;
				setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval));

				fptr = fopen(filename, "rb");	//Open the file to be sent

				if ((filesize % BUFSIZE) != 0)
					num_of_packets = (filesize / BUFSIZE) + 1;
				else
					num_of_packets = (filesize / BUFSIZE);

				printf("Number of packets to be sent: %d\n", num_of_packets);
                printf("File size: %lld\n", filesize);

				sendto(sockfd, &(num_of_packets), sizeof(num_of_packets), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
				recvfrom(sockfd, &(acknumb), sizeof(acknumb), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);
                
                // Advertise Number of packets, Retry set for 25 times 
                if (acknumb != num_of_packets)
				{
					for (j = 0; j < 25; j++) {
                        sendto(sockfd, &(num_of_packets), sizeof(num_of_packets), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
					    recvfrom(sockfd, &(acknumb), sizeof(acknumb), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length); 
                        if (acknumb == num_of_packets) {
                            break;
                        }
                    }
                    if (j == 25) {
                        printf("\n Error sending Num_of_packets\n");
                        break;
                    }
				}

				for (i = 1; i <= num_of_packets; i++)
				{
					memset(&packet, 0, sizeof(packet));
					acknumb = 0;
					packet.id = i;
					packet.pkt_len = fread(packet.data, 1, BUFSIZE, fptr);

					sendto(sockfd, &(packet), sizeof(packet), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
                    recvfrom(sockfd, &(acknumb), sizeof(acknumb), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);
                    printf("Ack - %ld, packetid - %ld", acknumb, packet.id);

                    // Retry for a packet until its corresponding ack is received
					if (acknumb != packet.id)
					{
                        //resend the packet and receive an ack again
						for (j = 0; j < 50; j++) {
                            sendto(sockfd, &(packet), sizeof(packet), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
                            recvfrom(sockfd, &(acknumb), sizeof(acknumb), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length);
                            printf("packet %ld dropped %d times\n", packet.id,j+1);
                            if (acknumb == packet.id){
                                break;
                            }
                        }
					}
                    if (j == 50) {
                        printf("\n Error sending packet #%ld \n", packet.id);
                        break;
                    }

					printf("packet sent - %d	Ack recvd - %ld\n", i, acknumb);
					if (num_of_packets == acknumb)
						printf("File sent\n");
				}
				fclose(fptr);
				
				t_out.tv_sec = 0;
				setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); 
            } else {
                printf("No File to PUT\n");
            }
            sleep(3);
        }
        else if ((strcmp(cmd, "get") == 0) && (filename[0] != '\0' )) {

			long int num_of_packets = 0;
			long int rx_bytes = 0, i = 0;

			t_out.tv_sec = 2;
			setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); 	

			recvfrom(sockfd, &(num_of_packets), sizeof(num_of_packets), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length); 
            t_out.tv_sec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); 	
			if (num_of_packets > 0) {
				sendto(sockfd, &(num_of_packets), sizeof(num_of_packets), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
				printf("Number of Packets: %ld\n", num_of_packets);
				
				fptr = fopen(filename, "wb");

				for (i = 1; i <= num_of_packets; i++)
				{
					memset(&packet, 0, sizeof(packet));

					recvfrom(sockfd, &(packet), sizeof(packet), 0, (struct sockaddr *) &clientaddr, (socklen_t *) &length); 
					sendto(sockfd, &(packet.id), sizeof(packet.id), 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
                    // code will wait to receive that packet again.
					if (packet.id != i)
						i--;
					else {
						fwrite(packet.data, 1, packet.pkt_len, fptr);   /*Write the recieved data to the file*/
						rx_bytes += packet.pkt_len;
					}

					if (i == num_of_packets) {
						printf("File received at client\n");
					}
				}
				printf("Bytes recieved: %ld\n", rx_bytes);
				fclose(fptr);
                sleep(3);
            } 
        }  else if ((strcmp(cmd, "del") == 0) && (filename[0] != '\0')) {

            length = sizeof(clientaddr);
            ack_rx = 0;
                                                                                                                                
            if((delack = recvfrom(sockfd, &(ack_rx), sizeof(ack_rx), 0,  (struct sockaddr *) &clientaddr, (socklen_t *) &length)) < 0)
                error("recieve");
            
            if (ack_rx == 3)
                printf("\nfile deleted\n");
            else if (ack_rx == 1)
                printf("\ngiven filename doesnt exist\n");
            else
                printf("\nno permission to delete\n");
        } else if (strcmp(cmd, "ls") == 0) {

            char filenames[200];
            memset(filenames, 0, sizeof(filenames));
            length = sizeof(clientaddr);

            if ((delack = recvfrom(sockfd, filenames, sizeof(filenames), 0,  (struct sockaddr *) &clientaddr, (socklen_t *) &length)) < 0)
                error("recieve");
            printf("\nOutput: \n %s\n", filenames);

        } else if (strcmp(cmd, "exit") == 0) {
			exit(0);
		} else {
            Usage();
        }
	} 
}