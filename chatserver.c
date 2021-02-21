 #include <stdio.h>
 #include <stdlib.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <string.h>
 #include <unistd.h>
 #include <fcntl.h>
 #include <signal.h>
 #include <errno.h>

#define BUFLEN 4096
#define MAX_ON_WELCOME 5
#define NUM 10

typedef struct Node 
{ 
    struct Node* next; 
    int write_fd;
    char text[BUFLEN]; 
}Node; 

typedef struct List 
{ 
    struct Node* head; 
    struct Node* tail; 
    int ListSize;
}List; 

List* Messages;


// hander for quiting the program
void sig_handler()
{
    if(Messages)
    {
        Node* p= Messages->head;
        while(p)
        {
            Messages->head=Messages->head->next;
            free(p);
            p=Messages->head;    
        }

        free(Messages);
    }    
    exit(EXIT_SUCCESS);
}

// the function delete sent messeges
void delete_messages(List* Messages, int fd)
{
    Node* p = Messages->head;

    //Empty Messages
    if(!p)
        return;
    

    //One node Messages
    if(Messages->ListSize == 1)
    {
        if(p->write_fd == fd)
        {
            Messages->head = NULL;
            Messages->tail = NULL;
            Messages->ListSize--;
            free(p);
            return;
        }
    }
    //Any other case
    while(p->next)
    {
        if(Messages->head == p && p->write_fd == fd)
        {
            Messages->head = Messages->head->next;
            free(p);
            p = Messages->head;
            Messages->ListSize--;
            continue;
        }
        else if(p->next->write_fd == fd)
        {  
            Node* to_free= p->next;    
            p->next = p->next->next;
            free(to_free);
            Messages->ListSize--;

            if(!p->next)
                Messages->tail=p;
            
        }
        p = p->next;
    } 
}


int main(int argc, char* argv[])
{
    if(argc != 2){
        fprintf(stderr,"Wrong input\n");
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if(port<=0)
    {
        fprintf(stderr,"Unknown port\n");
        exit(EXIT_FAILURE);    
    }


    int sockfd,newsockfd;      // socket

    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0){ 
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if(bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0){
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }

    listen(sockfd,MAX_ON_WELCOME);

    fd_set readfd;
    fd_set writefd;
    fd_set constfd;

    FD_ZERO(&constfd);
    FD_SET(sockfd,&constfd);

    int rc=0;
    char buff[BUFLEN]={0};

    Messages=(List*)calloc(1,sizeof(List));
    Messages->ListSize=0;
    Messages->head=NULL;
    Messages->tail=NULL;

    int fd_size=sockfd;
    char num[NUM]={0};

    signal(SIGINT, sig_handler);


    // read and write messages
    while(1)
    {
        readfd=constfd;
        if(Messages->ListSize==0)
            FD_ZERO(&writefd);
        else    
            writefd=constfd;

        rc=select(fd_size+1,&readfd,&writefd,NULL,NULL);
        if(rc<0)
        {
            fprintf(stderr,"Select Failed\n");
            exit(EXIT_FAILURE);
        }

        // add new client
        if(FD_ISSET(sockfd,&readfd))
        {
            newsockfd=accept(sockfd,NULL,NULL);
            if(newsockfd > 0)
            {   
                FD_SET(newsockfd,&constfd);
                if(newsockfd>fd_size)
                    fd_size=newsockfd;
            }
        }
        // READ
        for(int i = sockfd+1 ; i <= fd_size ; i++)
        {
            if(FD_ISSET(i,&readfd))
            {
                printf("Server is ready to read from socket %d\n",i); 
                rc=read(i,buff,BUFLEN);
                // client exit
                if(rc==0)
                {
                    close(i);
                    FD_CLR(i,&constfd);
                }
                // read message from client
                else if(rc>0)
                {
                    int j=sockfd+1;

                    while(j <= fd_size)
                    {
                        if(FD_ISSET(j,&constfd))
                        { 
                             if(j != i)
                            {
                                Node* P=(Node*)calloc(1,sizeof(Node));
                                P->write_fd=j;
                                strncpy(P->text,"guest",5);
                                sprintf(num,"%d",i);
                                strncpy(P->text+5,num,strlen(num));
                                strncpy(P->text+5+strlen(num),": ",2);
                                strncpy(P->text+7+strlen(num),buff,rc);

                                if(Messages->ListSize==0)
                                {
                                    Messages->head=P;
                                    Messages->tail=P;
                                }
                                else
                                {
                                    Messages->tail->next=P;
                                    Messages->tail= Messages->tail->next;
                                }

                                Messages->ListSize++;      
                            }

                        }
                        j++;
                    }
                }

                else
                {
                    perror("Read failed");
                    exit(EXIT_FAILURE);
                }
            }
        }

        // WRITE
         for(int i=sockfd+1 ; i<= fd_size ; i++)
         {
            if(Messages->ListSize > 0)
            {
                if(FD_ISSET(i,&writefd))
                {
                    Node* G=Messages->head;
                    while(G)
                    {
                        if(G->write_fd == i)
                        {
                            printf("Server is ready to write to socket %d\n",i);
                            write(G->write_fd,G->text,strlen(G->text)); 
                        }

                        G=G->next;
                    } 
                    delete_messages(Messages,i);   
                }
            }
        } 
     }
    return EXIT_SUCCESS;
}
