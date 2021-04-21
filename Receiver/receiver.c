#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include <sys/time.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>


#define PORT 9333
#define SERVER_IP[] "127.0.0.1"  
#define DATA_BUFFER 1000   //size of the data buffer
#define HEADER_SIZE 8 
#define WINDOW_SIZE 10
#define SLEEP_TIME 1
#define WINDOW_SENT_ACK -10  
#define EOF_ACK -99

//packet structure consisting of size, sequence number and data
struct PACKET{
       int DATA_size; 
       int seqNum;                       
       unsigned char DATA[DATA_BUFFER];                           
};

//creates packet and initializes all attributes
struct PACKET* createPacket(int seqNum, char* send_buff, int data_size){

    struct PACKET* packet = malloc(sizeof(struct PACKET));  
    packet->seqNum = seqNum;
    packet->DATA_size = data_size;
    if (send_buff != 0)
        memcpy(packet->DATA,send_buff,data_size);
    return packet;

}

void sendPacket(int sockfd, struct sockaddr_in addr, struct PACKET* packet){
    
    sendto(sockfd, (char *)packet, HEADER_SIZE + packet->DATA_size, 
            MSG_DONTWAIT, (const struct sockaddr *)&addr,   
            sizeof(addr));   //MSG_DONTWAIT flag set to enable nonblocking operation

}

struct PACKET* receivePacket(int sockfd, struct sockaddr_in *address){
    
    struct PACKET* packet = malloc(sizeof(struct PACKET));
    int len = sizeof(address);
    int read = recvfrom(sockfd, (char *)packet, sizeof(*packet),  
                MSG_DONTWAIT, (struct sockaddr *) address, 
                &len);
    
    if (read == -1){  
        free(packet);
        return NULL;
    }
    return packet;
}

void printString(const char* string){
    printf(string);
    printf("\n");
    fflush(stdout);
}

double cTime(){
    struct timeval time;
    double seconds = 0;
    gettimeofday(&time, NULL);
    seconds = (double)(time.tv_usec) / 1000000 + (double)(time.tv_sec);
    return seconds;
}

//prints sent number of bytes updating in real time
void bprint(int x,int y){
 printf("%c[%d;%df",0x1B,y,x);
}

int main(){

    char AckMsg[100] = "Packet Received";
    char NackMsg[100] = "Packet not Received";

    //Video data will be written to file receiverVideo.mp4
    char fileName[100] = "receiverVideo.mp4";

    //store packets in the window and initialize to 0
    struct PACKET* PACKETS_ARRAY[WINDOW_SIZE];
    memset(PACKETS_ARRAY,0,sizeof(PACKETS_ARRAY));
    struct sockaddr_in local, remote;
    int socket_id;

    system("clear");
    //creating udp socket 
    if((socket_id = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        fprintf(stderr, "Socket Error!\n");
        exit(-1);
    }
    else{
        printf("Socket Connected!\n");
    }

    //initializing socket variables 
    memset((char *) &remote, 0, sizeof(remote));
    memset((char *) &local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    //binding socket
    if(bind(socket_id, &local, sizeof(local)) == -1){
        fprintf(stderr, "Socket Binding Error!\n");
        exit(-1);
    }

    struct PACKET* receivingPacket;
    struct PACKET* sendingPacket;

    int expSeqNum = 0; 
   // double PACKET_SEND_TIME = 0; 
    receivingPacket = NULL;

    printString("Ready for receiving data ...");

    FILE* output;
    output = fopen(fileName,"ab");

    int while_1 = 1;  //Condition for while there is data to be received
    int Total_packets_received = 0;
    int Total_packets_written = 0;
    int Transfer_start_time = cTime();
    double long Transfer_end_time;
    int window_start_index = 0;
    int window_end_index = 10;
    double long Total_data_written = 0;
    int i;

    while(while_1 == 1){
        
        receivingPacket = receivePacket(socket_id,&remote);

	    //check if received packet is in window
        if(receivingPacket != NULL && receivingPacket->seqNum != EOF_ACK 
        && receivingPacket->seqNum != WINDOW_SENT_ACK && receivingPacket->seqNum >= window_start_index 
        && receivingPacket->seqNum <= window_end_index){
            Total_packets_received++;

	    //packet to send acknowledgment to sender
            sendingPacket = createPacket(receivingPacket->seqNum,AckMsg,receivingPacket->DATA_size);
            sendPacket(socket_id,remote,sendingPacket);

	        if(receivingPacket->seqNum == expSeqNum){
	    	    expSeqNum++;
	    	    window_start_index++;
            	window_end_index++;
	        }

	        else{
		        sendingPacket = createPacket(expSeqNum,NackMsg,strlen(NackMsg));  //sending Nack message
                sendPacket(socket_id,remote,sendingPacket);
	        }

            fflush(stdout);
          
	        //Check for duplicate packet then add to array 
            if(PACKETS_ARRAY[receivingPacket->seqNum % 10] == 0){
                 PACKETS_ARRAY[receivingPacket->seqNum % 10] = receivingPacket;
            }

            else{
                printf("Duplicate Packet: %d\n",receivingPacket->seqNum);
            }
        }
       
        //Check if Recieved Packet Signals Window has been sent and send ack
        if(receivingPacket != NULL && receivingPacket->seqNum == WINDOW_SENT_ACK){

            sendingPacket = createPacket(WINDOW_SENT_ACK,NULL,NULL);
            sendPacket(socket_id,remote,sendingPacket);

	        //Write data to file if packets contain data
            for(i = 0;i < WINDOW_SIZE;i++){
                if(PACKETS_ARRAY[i] != 0){
                    if(PACKETS_ARRAY[i]->DATA != 0 && PACKETS_ARRAY[i]->DATA != NULL){

                        fwrite(PACKETS_ARRAY[i]->DATA, 1, PACKETS_ARRAY[i]->DATA_size, output);
                        Total_packets_written++;
                        Total_data_written += PACKETS_ARRAY[i]->DATA_size;

                        bprint(0,4);
                        printf("Received File: %llf MB\n",Total_data_written / 1000000);

                    }
                }
            }

	        //empty buffer
            memset(PACKETS_ARRAY,0,sizeof(PACKETS_ARRAY));
        }

	    //Check packet for EOF and send packet for ack
        if(receivingPacket != NULL && receivingPacket->seqNum == EOF_ACK){

            sendingPacket = createPacket(EOF_ACK,NULL,NULL);
            sendPacket(socket_id,remote,sendingPacket);
	    
	        //write data to file if packets contain data
            for(i = 0;i < WINDOW_SIZE;i++){
                if(PACKETS_ARRAY[i] != 0){
                    if(PACKETS_ARRAY[i]->DATA != 0 && PACKETS_ARRAY[i]->DATA != NULL){
                        fwrite(PACKETS_ARRAY[i]->DATA, 1, PACKETS_ARRAY[i]->DATA_size, output);
                        Total_packets_written++;
                        Total_data_written += PACKETS_ARRAY[i]->DATA_size;
                    }
                }
            }

	    //empty buffer
            memset(PACKETS_ARRAY,0,sizeof(PACKETS_ARRAY));
            while_1 = 0;
            fflush(stdout);
        }

        usleep(SLEEP_TIME);
    }


    //Print transfer details onto screen 
    printf("\n");
    printf("---------------------------- RECEIVER ----------------------------\n");

    Transfer_end_time = cTime() - Transfer_start_time;

    if(Transfer_end_time < 60){
        printf("Total Transfer Time: %llf Sec\n",Transfer_end_time);
    }else{
        printf("Total Transfer Time: %llf Min\n",Transfer_end_time / 60);
    }

    printf("Total Packets Received: %d\n",Total_packets_received);
    printf("Total Packets Written: %d\n",Total_packets_written);
    printf("Total Data Written: %llf Mb\n",Total_data_written / 1000000);

}
