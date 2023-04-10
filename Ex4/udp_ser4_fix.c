#include "headsock.h"

void readsocket(int sockfd);                                                                        // Receive data from socket sockfd
void send_ack(int sockfd, struct sockaddr *client_addr, socklen_t client_addrlen, long fileoffset); // Block until acknowledge is sent

int main(int argc, char *argv[])
{
    // Setup server socket address
    struct sockaddr_in server_addr_in;
    server_addr_in.sin_family = AF_INET;
    server_addr_in.sin_port = htons(MYUDP_PORT); // htons() converts port number to big endian
    server_addr_in.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY=0, 0 means receive data from any IP address
    memset(&(server_addr_in.sin_zero), 0, sizeof(server_addr_in.sin_zero));
    // Typecast internet socket address (struct sockaddr_in) to generic socket address (struct sockaddr)
    struct sockaddr *server_addr = (struct sockaddr *)&server_addr_in;
    socklen_t server_addrlen = sizeof(struct sockaddr);

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        printf("error creating socket");
        exit(1);
    }

    // Bind socket address to socket
    if (bind(sockfd, server_addr, server_addrlen) == -1)
    {
        printf("error in binding");
        exit(1);
    }

    // Read file data from socket
    while (1)
    {
        printf("Ready to receive data\n");
        readsocket(sockfd);
    }
    close(sockfd);
}

void readsocket(int sockfd)
{

    char packet[DATALEN];

    // Create empty struct to contain client address
    // It will be filled in by recvfrom()
    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(struct sockaddr);

    // Get filesize from client
    long filesize;
    int n = recvfrom(sockfd, &filesize, sizeof(filesize), 0, &client_addr, &client_addrlen);
    if (n < 0)
    {
        printf("error in receiving packet\n");
        exit(1);
    }
    send_ack(sockfd, &client_addr, client_addrlen, 0); // Acknowledge that filesize has been received
    printf("File is %ld bytes big. Dataunit size %d bytes\n", filesize - 1, DATALEN);

    char filebuffer[filesize];

    int multiples = 2;
    long fileoffset = 0; // Tracks how many bytes have been received so far
    char packetlastbyte; // Tracks the last byte of the latest packet received
    do
    {
        // varying batch protocol
        for (int i = 0; i < multiples; i++)
        {
            // Read incoming packet (recvfrom will block until data is received)
            int bytesreceived = recvfrom(sockfd, &packet, DATALEN, 0, &client_addr, &client_addrlen);
            if (bytesreceived < 0)
            {
                printf("error in receiving packet\n");
                exit(1);
            }

            // Append packet data to filebuffer
            memcpy((filebuffer + fileoffset), packet, bytesreceived);
            fileoffset += bytesreceived;
            if ((packetlastbyte = packet[bytesreceived - 1]) == 0x4)
                break;
        }

        // Acknowledge that packet has been received
        send_ack(sockfd, &client_addr, client_addrlen, fileoffset);

    } while (packetlastbyte != 0x4);
    fileoffset -= 1; // Disregard the last byte of filebuffer because it is the End of Transmission character 0x4

    // Open file for writing
    char filename[] = "receive.txt";
    FILE *fp = fopen(filename, "wt");
    if (fp == NULL)
    {
        printf("File %s doesn't exist\n", filename);
        exit(1);
    }

    // Copy the filebuffer contents into file
    fwrite(filebuffer, 1, fileoffset, fp);
    fclose(fp);
    printf("File data received successfully, %d bytes written\n", (int)fileoffset);
}

void send_ack(int sockfd, struct sockaddr *addr, socklen_t addrlen, long fileoffset)
{
    const int ACK = 1;
    int ack_sent = 0;
    int ack_threshold = 10;
    while (!ack_sent)
    {
        if (sendto(sockfd, &ACK, sizeof(ACK), 0, addr, addrlen) >= 0)
        {
            ack_sent = 1;
        }
        else
        {
            if (ack_threshold-- <= 0)
            {
                printf("Failed to send acknowedgment");
                exit(1);
            }
            else
                printf("error sending ack, trying again..\n");
        }
    }
}