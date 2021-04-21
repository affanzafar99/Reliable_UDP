//include librarires
#include<arpa/inet.h>
#include<netinet/in.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<string.h>
#include <sys/time.h>
#include <stdbool.h>

//define constants 
#define PORT 9333
#define SERVER_IP[] "127.0.0.1"
#define DATA_BUFFER_SIZE 500
#define HEADER_SIZE 8  
#define WINDOW_SIZE 10
#define WINDOW_SENT_ACK -10
#define END_OF_FILE_ACK -99
#define SLEEP_TIME 0.01

//make a structure for the packets which includes size, sequence number and data
struct PACKET{  
       int length; 
       int seqNum;                       
       unsigned char DATA[DATA_BUFFER_SIZE];                           
};


//gotoxy function prints the number of bytes sent and updates in real time
void gotoxy(int x,int y)
{
 printf("%c[%d;%df",0x1B,y,x);
}

double calculate_time(){
    struct timeval time;
    double seconds = 0;
    gettimeofday(&time, NULL);
    seconds = (double)(time.tv_usec) / 1000000 + (double)(time.tv_sec);
    return seconds;
}

//Prints text on screen
void printString(const char* string){
    printf(string);
    printf("\n");
    fflush(stdout);
}


//Function create packet
//returns a packet with all attributes initialized
struct PACKET* create_packet(int seqNum, char* send_buff, int data_size){
    //create variable of type packet and allocate memory
    struct PACKET* packet = malloc(sizeof(struct PACKET));
    //initialize attributes
    packet->seqNum = seqNum;
    packet->length = data_size;
    if (send_buff != 0)
        memcpy(packet->DATA,send_buff,data_size);
    return packet;
}


//function send packet
void send_packet(int sockfd, struct sockaddr_in addr, struct PACKET* packet){
    //calls function sendto
    //flag set to MSG_DONTWAIT for non blocking send
    sendto(sockfd, (char *)packet, HEADER_SIZE + packet->length, 
        MSG_DONTWAIT, (const struct sockaddr *)&addr,  
            sizeof(addr)); 
}


//function receive packet
struct PACKET* receivePacket(int sockfd, struct sockaddr_in *addr){
    //allocate memory for packet
    struct PACKET* packet = malloc(sizeof(struct PACKET));
    int length = sizeof(addr);
    int nread = recvfrom(sockfd, (char *)packet, sizeof(*packet),  
                MSG_DONTWAIT, (struct sockaddr *)&addr, 
                &length);
    //if no bytes are read
    if (nread == -1){
        free(packet);
        return NULL;
    }
    return packet;
}


//main program
int main(int argc, char *argv[]){
    
    //make an array of packets to store packets in the window and initialize to 0
    struct PACKET* PACKETS_ARRAY[WINDOW_SIZE] ;
    memset(PACKETS_ARRAY,0,sizeof(PACKETS_ARRAY));

    //make an array to store ack packets
    bool acks[WINDOW_SIZE] = {false};

    char file_name[100];
    struct sockaddr_in cli_addr; 
    int Socket_id;
    int Total_bytes_read; 
   
    system("clear");
    //check if file name is provided. otherwise ask user for file name
    if (argc < 2) {
        printf("Enter the Path to video file to send: ");
            gets(file_name);
    }
    else{
        strcpy(file_name,argv[1]);
    }


    //create udp socket and check if it has been created 
    Socket_id = socket(AF_INET, SOCK_DGRAM, 0);
    if(Socket_id == -1){
        fprintf(stderr, "Error getting a socket! \n");
        exit(-1);
    }

    //initialize socket variables 
    memset((char *) &cli_addr, 0, sizeof(cli_addr));
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_port = htons(PORT);
    cli_addr.sin_addr.s_addr = INADDR_ANY;


    struct PACKET* receiving_packet = NULL;
    struct PACKET* sending_packet;
    
    //open video file for reading data
    FILE *file;
    file = fopen(file_name,"rb");
    if(file == NULL){
            printf("Error Opening the file\n");
            return 1;   
    }
    
    
    //initialize variables to be used to make packets 
    int Total_packets_made = 0;
    int Total_packets_sent = 0;
    int Condition_while_1 = 1;
    int Condition_sending_while = 1;
    int Total_packets_sent_in_single_window = 0;
    int window_num = 0;
    double long Data_sent_size = 0;
    int Total_ack_received = 0;
    int Condition_timeout_while = 0;
    int Total_duplicate_sent = 0;
    int i = 0;    
    int seqNum=0;
    int window_start = 0;
    int window_end = 10;
    int expectedACK_number=0;

    int transfer_start_time = calculate_time();

    unsigned char buffer[DATA_BUFFER_SIZE];

    //while there is data in the file to be sent 
    while(Condition_while_1 == 1){
        for(i = 0;i < WINDOW_SIZE;i++){

                //read 400 bytes from file and write to buffer
                Total_bytes_read = fread(buffer, 1, DATA_BUFFER_SIZE, file);

   		//if any data was read, make packet with that data
                if(Total_bytes_read > 0){
		    sending_packet = create_packet(seqNum,buffer,Total_bytes_read);
                    Total_packets_made++;
                    fflush(stdout);
                    PACKETS_ARRAY[sending_packet->seqNum % 10] = sending_packet;
                    seqNum++;
                } //end if
        }  //end for

        Condition_sending_while = 1;
        int Total_packets_sent_for_window = 0;
        int Total_acks_received_for_window = 0;

        //while there are unacknowledged packets, stop and wait for acks
        while(Condition_sending_while == 1){
            fflush(stdout);

            for(i = 0;i < WINDOW_SIZE;i++){
		//make packets from data in packet array
                if(PACKETS_ARRAY[i] != 0 && acks[i] == false){
                    
    		    //send packets
                    send_packet(Socket_id,cli_addr,PACKETS_ARRAY[i]);

		    //calculate and print progress of sending file
                        Data_sent_size += PACKETS_ARRAY[i]-> length;
                        gotoxy(0,4);
                        printf("File Sent: %llf MB\n",Data_sent_size / 1000000);
                        fflush(stdout);

                    Total_packets_sent_for_window++;
                    Total_packets_sent++;
                    Total_packets_sent_in_single_window++;
                    fflush(stdout);
                } //end if
	    }  //end for

	//send and recieve all packets of one window before moving to the next (STOP AND WAIT)
	while(window_start < window_end){ 

		//if a packet is recieved
		if(receiving_packet = receivePacket(Socket_id,&cli_addr)){ 

		//save ack for packet using sequence number to index into the ack array
                   if(receiving_packet->seqNum >= window_start && receiving_packet->seqNum <= window_end && strcmp(receiving_packet->DATA, "Packet recieved")){ 
                        acks[receiving_packet->seqNum % 10] = true;
                        Total_ack_received++;
                        Total_acks_received_for_window++;

			//if packet with expected sequence number is recieved, move front of window forward
			if(receiving_packet->seqNum == expectedACK_number){
			expectedACK_number++;
			window_start++;
			}

                 }  //end if
		}  //end if

	if(feof(file)) break;

	} //end while

            //check if all packets in the window have been acked
	    Condition_sending_while = 0;	
	    for(i = 0;i < WINDOW_SIZE;i++){
               		 if(acks[i] == false && !feof(file)){
			 //retransmit if all packets not acked
			    Condition_sending_while = 1;
			 } //end if
	    usleep(SLEEP_TIME);

		 
	    }  //end for
	}  //end_condition_sending_while_1
         

	//move window forward by window size
	window_end= window_end +10;


        //if all packets have been acked, make and send packed indicating the window has been sent
        sending_packet = create_packet(WINDOW_SENT_ACK,NULL,NULL);
        int WINDOW_SENT_ACK_ACK = -10;
        while(WINDOW_SENT_ACK_ACK == -10){ 
        	send_packet(Socket_id,cli_addr,sending_packet);
                    
		usleep(SLEEP_TIME);

		//receive ack for window sent packet
                while(receiving_packet = receivePacket(Socket_id,&cli_addr)){
                	if(receiving_packet->seqNum == WINDOW_SENT_ACK){
                        	WINDOW_SENT_ACK_ACK = 0;
               		}  //end if
		} //end while	
        usleep(SLEEP_TIME);		
	}  //end while window sent ack

        
        if(feof(file)){
		sending_packet = create_packet(END_OF_FILE_ACK,NULL,NULL);

                int END_OF_FILE_ACK_ACK = -99;
                     
                while(END_OF_FILE_ACK_ACK == -99){   
                	send_packet(Socket_id, cli_addr, sending_packet);
                        while(receiving_packet = receivePacket(Socket_id,&cli_addr)){
                            if(receiving_packet->seqNum == END_OF_FILE_ACK){
                                END_OF_FILE_ACK_ACK = 0;
				Condition_while_1 = 0;
                            }  //end if
			  
                            usleep(SLEEP_TIME);
                        }  //end while

                } //end while end of file
                if(ferror(file)){
                	printf("Error Reading\n");      
                }
        } //end if feof

        if(Total_packets_sent_in_single_window > 10){
            Total_duplicate_sent += Total_packets_sent_in_single_window - 10;
        }
        Total_packets_sent_in_single_window = 0;
        window_num++;
	
        memset(PACKETS_ARRAY,0,sizeof(PACKETS_ARRAY));
        memset(acks,false,sizeof(acks));
        usleep(SLEEP_TIME);
    } //end while_condition_1

    double long transfer_end_time = calculate_time() - transfer_start_time;

    printf("\n");
    printf("========================= TRANSFER SUMMARY ==========================\n");

    if(transfer_end_time < 60){
        printf("Total Transfer Time: %llf Sec\n",transfer_end_time);
    }else{
        printf("Total Transfer Time: %llf Min\n",transfer_end_time / 60);
    }

    printf("Total Packets made: %d\n",Total_packets_made);
    printf("Total Packets Sent: %d\n",Total_packets_sent);
    printf("Total Duplicate Packets Sent: %d\n",Total_duplicate_sent);
    printf("Total Acks Received: %d\n",Total_ack_received);
    printf("Total Data Sent: %llf Mb at the Rate of %llf Mb/s\n",(Data_sent_size / 1000000),(Data_sent_size / 1000000) / transfer_end_time);
    return 0;
}
