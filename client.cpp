#include <iostream>
#include <vector>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "data.h"
#include "ack.h"
using namespace std;

#define PORT (8888)
#define MAX_LEN (1024)
int run = 1;
const char *end = "this is last message";
struct sockaddr_in seraddr;
struct sockaddr_in chat_ser;
struct sockaddr_in cliaddr;
int CLIPORT;
int len = sizeof(struct sockaddr_in);

int init_sock(int port, const char *addr)
{
    int sockfd = 0;

    cout << "input port" << endl;
    cin >> CLIPORT;
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    memset(&seraddr,0X00,len);
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(port);
    inet_aton("127.0.0.1",&seraddr.sin_addr);
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(CLIPORT);
    inet_aton(addr,&cliaddr.sin_addr);
    if(0 > bind(sockfd,(const struct sockaddr *)&cliaddr,len)){
        printf("wrong\n");
        return -1;
    }
    //connect(sockfd,(const struct sockaddr *)&seraddr,len);

    return sockfd;
}

int ten_mi(int combo)
{
    int num = 1;
    for(int i = 0; i < combo; ++i){
        num *= 10;
    }
    return num;
}
char *int_to_str(int num)
{
    char *str = new char[4];
    for(int i = 0; i < 4; ++i){
        str[i] = num/(ten_mi(3-i))%10 + '0';
    }
    return str;
}
void deal_data(int sockfd)
{
    char recbuf[MAX_LEN] = {0};
    char senbuf[MAX_LEN] = {0};
    pid_t chid = 0;
    struct data_struct data;
    struct sockaddr_in new_addr;
    int recv_id, send_id;
    char passwd[5] = {',','1','2','3','\0'};
    socklen_t len = sizeof(struct sockaddr);

    memset(&data, 0x00, sizeof(data));
    cout << "please cin recv_id:" << endl;
    cin >> recv_id;
    cout << "please cin send_id:" << endl;
    cin >> send_id;
    data.send_id = send_id;
    data.recv_id = recv_id;
    char *num = int_to_str(send_id);
    memcpy(data.buf, num, strlen(num));
    strcat(data.buf, passwd);
    cout << data.buf << endl;
    sendto(sockfd,&data,sizeof(struct data_struct),0,(const struct sockaddr *)&seraddr,len);
    while(1){
        recvfrom(sockfd,&data,sizeof(struct data_struct),0,(struct sockaddr *)&new_addr,&len);
        if(new_addr.sin_port != seraddr.sin_port){
            memcpy(&chat_ser, &new_addr, len);
            break;
        }
    }
    chid = fork();
    if(0 == chid){
        while(run){
            memset(&data, 0x00, sizeof(data));
            recvfrom(sockfd,&data,sizeof(struct data_struct),0,(struct sockaddr *)&chat_ser,&len);
            //recv(sockfd,recbuf,MAX_LEN-1,0);
            sync();
            printf("server say: %s\n",data.buf);
        }
        exit(0);
    }else{
        memset(&data, 0x00, sizeof(struct data_struct));
        //data.begin_flag = 1;
        data.send_id = send_id;
        data.recv_id = recv_id;
        printf("my:>>");
        fflush(stdout);
        read(0, data.buf, MAX_LEN-1);
        sendto(sockfd,&data,sizeof(struct data_struct),0,(const struct sockaddr *)&chat_ser,len);
        while(1){
            data.send_id = send_id;
            data.recv_id = recv_id;
            printf("my:>>");
            fflush(stdout);
            read(0,data.buf,MAX_LEN-1);
            if(!strcmp(senbuf,end)){
                kill(chid,SIGQUIT);
                break;
            }
            sendto(sockfd,&data,sizeof(struct data_struct),0,(const struct sockaddr *)&chat_ser,len);
            //send(sockfd,senbuf,MAX_LEN-1,0);
            memset(senbuf,0X00,MAX_LEN);
        }
        wait(0);
    }
}
void sig_end(int sig)
{
    run = 0;
}
int main(int ac,char *av[])
{
    int sockfd = 0;

    //if(ac <= 1){
    //    printf("wrong arg\n");
    //    return 1;
    //}
    signal(SIGQUIT,sig_end);
    sockfd = init_sock(PORT,"192.168.3.42");
    deal_data(sockfd);
    close(sockfd);

    return 0;
}
