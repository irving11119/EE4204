#include "headsock.h"
#include <arpa/inet.h>

void sendsocket(int sockfd, struct sockaddr *addr, socklen_t addrlen); // Send data to socket address
void tv_sub(struct timeval *out, struct timeval *in);                  // Time taken for transmission to complete
void wait_ack(int sockfd, struct sockaddr *addr, socklen_t addrlen);   // Block until acknowledge is received

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Please provide an IP address. Example: 127.0.0.1 -> this is localhost\n");
        exit(1);
    }
    char *IP_addr = argv[1];

    // Setup server socket address
    struct sockaddr_in server_addr_in;
    server_addr_in.sin_family = AF_INET;
    server_addr_in.sin_port = htons(MYUDP_PORT);         // htons() converts port number to big endian form
    server_addr_in.sin_addr.s_addr = inet_addr(IP_addr); // inet_addr converts IP_addr string to big endian form
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

    // Send file data to socket
    sendsocket(sockfd, server_addr, server_addrlen);
    close(sockfd);
}

void sendsocket(int sockfd, struct sockaddr *server_addr, socklen_t server_addrlen)
{
    //--------------------------------------------//
    // buffer used to contain the outgoing packet //
    //--------------------------------------------//
    char packet[DATALEN];

    // Open file for reading
    char filename[] = "myfile.txt";
    FILE *fp = fopen(filename, "rt");
    if (fp == NULL)
    {
        printf("File %s doesn't exist\n", filename);
        exit(1);
    }

    // Get the total filesize
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);
    printf("The file length is %d bytes\n", (int)filesize);

    char filebuffer[filesize];

    // Copy the file contents into filebuffer
    fread(filebuffer, 1, filesize, fp);
    filebuffer[filesize] = 0x4; // EOF character
    filesize += 1;              // Accomodate EOF
    fclose(fp);

    sendto(sockfd, &filesize, sizeof(filesize), 0, server_addr, server_addrlen); // sends file size to sever
    wait_ack(sockfd, server_addr, server_addrlen);                               // waits for ack that server knows file size

    // Get start time
    struct timeval timeSend;
    gettimeofday(&timeSend, NULL);

    long byteswritten = 0; // Tracks how many bytes have been sent so far
    int multiples = 2;     // data unit multiple, alternates between 1,2,3

    while (byteswritten < filesize)
    {
        // Depending on dum, send packets of DATAUNIT size 1, 2, or 3 times before waiting for an ACK
        for (int i = 0; i < multiples; i++)
        {
            int packetsize = (DATALEN < filesize - byteswritten) ? DATALEN : filesize - byteswritten; // packetsize should never be bigger than what's left to send
            memcpy(packet, (filebuffer + byteswritten), packetsize);                                  // Copy the next section of the filebuffer into the packet
            byteswritten += packetsize;                                                               // Update fileoffset
            int n = sendto(sockfd, &packet, packetsize, 0, server_addr, server_addrlen);              // Send packet data into socket
            if (n < 0)
            {
                exit(1);
            }
            printf("packet of size %d sent\n", n);
        }
        wait_ack(sockfd, server_addr, server_addrlen);
    }

    // Get end time
    struct timeval timeRcv;
    gettimeofday(&timeRcv, NULL);

    // Calculate difference between start and end time and print transfer rate
    tv_sub(&timeRcv, &timeSend);
    float time = (timeRcv.tv_sec) * 1000.0 + (timeRcv.tv_usec) / 1000.0;
    float throughput = byteswritten / time / 1000; // in Megabytes
    printf("Dataunit size %d bytes\n", DATALEN);
    printf("%ld bytes sent in %.3fms | Throughput: %.3f Mbytes/s\n", byteswritten, time, throughput);
}

void tv_sub(struct timeval *out, struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0)
    {
        --out->tv_sec;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}

void wait_ack(int sockfd, struct sockaddr *addr, socklen_t addrlen)
{
    int ack_received = 0;
    int ACKNOWLEDGE = 0;
    while (!ack_received)
    {
        if (recvfrom(sockfd, &ACKNOWLEDGE, sizeof(ACKNOWLEDGE), 0, addr, &addrlen) >= 0)
        {
            if (ACKNOWLEDGE == 1)
            {
                ack_received = 1;
                printf("ACKNOWLEDGE received\n");
            }
            else
            {
                printf("ACKNOWLEDGE received but value was not 1\n");
                exit(1);
            }
        }
        else
        {
            printf("error when waiting for acknowledge\n");
            exit(1);
        }
    }
}