#ifndef _CHATSER_H
#define _CHATSER_H

#include <iostream>
#include <new>
#include <map>
#include <vector>
#include <queue>
#include <sys/epoll.h>
#include <exception>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include "database.h"
#include "data.h"
#include "ack.h"
using namespace std;

#define __THROW_BAD_SOCK cerr << "error socket" << endl; exit(1);
#define SER_PORT        (8888)
#define CHAT_PORT       (8889)
#define CLI_PORT        (6666)
#define MAX_MESSAGE_LEN (1024)
#define MAX_OLD_NUM     (99)
#define CONNECT_COUNT   (10000)
#define NOT_ONLINE      (0)       //不在线
#define ONLINE          (1)       //在线
#define TH_BUSY         (0)       //线程忙
#define TH_IDEL         (1)       //线程闲
#define TH_NUM          (4)       //创建4个线程

class SerSocket;
struct thread_para
{
    SerSocket *s;
    union
    {
        int *num1;
        int num2;
    };
};

union Index
{
    int *num1;
    int num2;
};
//类中封装线程函数的方法
//模板函数：
//其实传入的参数类型还是void *(fun)(void *)类型，
//不过在上面会加入另外的类型
//里面执行的是要执行的函数
template <typename TYPE, void (TYPE::*thread_deal_ev)(int)>
void *thread_fun(void *para)
{
    TYPE *This = ((thread_para *)para)->s;
    int num;

    num = ((thread_para *)para)->num2;
    This->thread_deal_ev(num);

    return NULL;
}

template <typename TYPE, void (TYPE::*thread_get_mutex)(int)>
void *thread_cond_fun(void *para)
{
    TYPE *This = ((thread_para *)para)->s;
    int num;

    num = ((thread_para *)para)->num2;
    This->thread_get_mutex(num);

    return NULL;
}

struct thread_info//线程的信息
{
    pthread_t tid;//线程id
    int state;//线程的状态，是空闲还是忙
    struct data_struct data;//data数据
};

void sig_usr(int sig)
{
}
//处理除了认证的所有事件的线程
//目前先处理聊天的收发
//解决服务器发送数据到客户端的问题：
//初步：
//创建一个map容器，存放新加入的客户端的信息，发送信息的时候去匹配
//map的key值就是id号， 对应的value是客户端的信息
class SerSocket
{
public:
    SerSocket(const char *ip):serip((char *)ip)
    {/*{{{*/
        socklen_t len = sizeof(struct sockaddr);
        thread_para p[TH_NUM];

        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        port = SER_PORT;
        if(sockfd < 0){
            __THROW_BAD_SOCK
        }
        data_len = sizeof(data_struct);
        memset(&seraddr, 0x00, sizeof(struct sockaddr_in));
        db = new Msql((char *)"root",(char *)"mayan1314",(char *)"127.0.0.1",(char *)"chatsql");
        seraddr.sin_family = AF_INET;
        inet_aton(serip,&seraddr.sin_addr);
        seraddr.sin_port = htons(port);

        if(0 > bind(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr))){
            close(sockfd);
            __THROW_BAD_SOCK
        }
        pthread_mutex_init(&mutex_state, NULL);
        pthread_mutex_init(&mutex_que, NULL);
        //创建线程
        for(int i = 0; i < TH_NUM; ++i){
            p[i].s = this;
            p[i].num1 = (int *)i;
            pthread_create(&(th_info[i].tid), NULL, thread_fun<SerSocket, &SerSocket::thread_deal_ev>, &p[i]);
        }
    }/*}}}*/
    ~SerSocket()
    {/*{{{*/
        close(sockfd);
    }/*}}}*/
public:
    //认证成功，创建一个新的套接字，把新套接字的信息发送到client端。
    //把套接字挂到epoll上，循环等待就绪
    void service()//暴露给外界的方法，其他都封装在这个函数里了
    {
        epoll_event events[1024];
        struct sockaddr i;
        int sz = 0;
        int chat_soci;
        int fd, chat_sockfd;
        int id;
        socklen_t len = sizeof(struct sockaddr);
        int data_len = sizeof(data);
        //recvfrom(sockfd, &data, data_len, 0, (struct sockaddr *)&cliaddr, &len);

        epollfd = epoll_create(1024);
        add_event(epollfd, sockfd, EPOLLIN);
        for(;;){
            sz = epoll_wait(epollfd, events, 1024, -1);
            if(sz < 0){
                perror("epollwait:");
                exit(1);
            }
            for(int i = 0; i < sz; ++i){
                fd = events[i].data.fd;
                if((fd == sockfd) && (events[i].events & EPOLLIN)){
                    //先过滤掉认证
                    //认证
                    id = certification();
                    //memset(&cliaddr, 0x00, sizeof(cliaddr));
                    //recvfrom(fd, &data, data_len, 0, (struct sockaddr *)&cliaddr, &len);
                    //cli_info_map[data.send_id] = cliaddr;
                    //id = data.send_id;
                    if(id == -1){
                        //认证失败
                        data.login_flag = false;
                        sendto(sockfd, &data, data_len, 0, (struct sockaddr *)&cliaddr, len);
                    }
                    else{
                        //发送离线数据
                        //send_old_data(id);
                        //创建新的套接字，并且告知客户端
                        chat_sockfd = csocket(id);
                        cli_fd_map[data.send_id] = chat_sockfd;
                        //加入套接字
                        add_event(epollfd, chat_sockfd, EPOLLIN);
                    }
                }else if(events[i].events & EPOLLIN){
                    memset(&data, 0x00, sizeof(data_struct));
                    //读取data后，交给线程池进行处理
                    SerSocket   *ser;
                    struct sockaddr_in sender_cli, recver_cli;
                    memset(&data, 0x00, sizeof(struct data_struct));
                    recvfrom(fd, &data, data_len, 0, (struct sockaddr *)&sender_cli, &len);
                    handle_event(&data);
                }
            }
        }
    }


    //处理接受到data的线程
    void thread_deal_ev(int num)
    {
        SerSocket   *ser;
        Index in;
        int data_len = sizeof(data);
        struct sockaddr_in sender_cli, recver_cli;
        socklen_t len = sizeof(sender_cli);
        bool empty;
        data_struct data;

        signal(SIGUSR1, sig_usr);
        th_info[num].state = TH_IDEL;
        while(1){
            //等条件
            pause();
            memcpy(&data, &(th_info[num].data), sizeof(data));
            //设置状态为忙
            th_info[num].state = TH_BUSY;
            while(1){
                if(data.send_id){//如果发送段发送了数据
                    if(&cli_info_map[data.recv_id] == 0){//如果接受端没有上线
                        db->add_data(data.recv_id, NOT_ONLINE, data.buf);
                    }else{
                        recver_cli = cli_info_map[data.recv_id];
                        sendto(cli_fd_map[data.recv_id], &data, data_len, 0, (struct sockaddr *)&recver_cli, len);
                        cout << data.buf << endl;
                        db->add_data(data.recv_id, ONLINE, data.buf);//把数据存到数据库，这里其实可以优化的
                    }
                }
                //查看队列是否要执行任务
                //加锁
                pthread_mutex_lock(&(mutex_que));
                empty = wait_que.empty();//查看队列状态
                if(empty){
                    pthread_mutex_unlock(&mutex_que);
                    break;
                }
                th_info[num].data = (wait_que).front();
                (wait_que).pop();
                pthread_mutex_unlock(&mutex_que);
            }
            //设置状态为闲
            th_info[num].state = TH_IDEL;
        }
    }

protected:
    void handle_event(struct data_struct *data)
    {
        int i = 0;

        for(i = 0; i < TH_NUM; ++i){
            //不需要加锁，因为读取本来就是原子操作
            if(th_info[i].state == TH_IDEL){//如果线程空闲,唤醒对应的线程，并且把状态置为忙
                memcpy(&(th_info[i].data), data, sizeof(struct data_struct));
                //触发条件，放给线程池执行
                pthread_kill(th_info[i].tid, SIGUSR1);
                break;
            }
        }
        if(i == TH_NUM){//线程用完了,把数据挂到等待的队列上
            pthread_mutex_lock(&mutex_que);
            wait_que.push(*data);
            pthread_mutex_unlock(&mutex_que);
        }
    }

    //创建新的套接字并且发送给客户端
    int csocket(int id)
    {
        int new_sockfd = 0;
        struct sockaddr_in chat_ser;
        struct sockaddr_in *client;
        int port = 0;
        data_struct d;
        socklen_t len = sizeof(chat_ser);

        new_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        //计算一个新的端口
        port = get_new_port();
        chat_ser.sin_family = AF_INET;
        chat_ser.sin_port = htons(port);
        chat_ser.sin_addr.s_addr = inet_addr("127.0.0.1");
        socklen_t socklen = sizeof(struct sockaddr);
        if(0 > bind(new_sockfd, (struct sockaddr *)&chat_ser, socklen)){
            perror("bind");
            //没想到好方法，暂不处理
        }
        memset(&d, 0x00, sizeof(data_struct));
        d.chat_port = port;
        client = &cli_info_map[id];
        //认证成功
        d.login_flag = true;
        sendto(new_sockfd, &d, data_len, 0, (struct sockaddr *)client, len);

        return new_sockfd;
    }

    //增加用户
    void add_event(int epollfd, int sockfd, int event)
    {
        epoll_event ev;
        ev.events = event;
        ev.data.fd = sockfd;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
    }

    //删除用户
    void del_event(int epollfd, int sockfd, int event)
    {
        epoll_event ev;
        ev.events = event;
        ev.data.fd = sockfd;
        epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &ev);
    }

    //认证
    int certification()
    {
        char *user;
        char *pwd;
        socklen_t len = sizeof(struct sockaddr);

        recvfrom(sockfd, &data, data_len, 0, (struct sockaddr *)&cliaddr, &len);
        //if need to create new id, to create.
        //and then to certification
        if(data.begin_flag == true){
            user = strtok(data.buf, ",");
            pwd = strtok(NULL, ",");
            memset(&data, 0x00, sizeof(data_struct));
            if(0 > (id = db->add_user_pwd(user, pwd))){
                sendto(sockfd, &data,data_len, 0, (struct sockaddr *)&cliaddr, len);
            }
            else{
                data.new_id = id;
                sendto(sockfd, &data, data_len, 0, (struct sockaddr *)&cliaddr, len);
                memset(data.buf, 0x00, MAX_MESSAGE_LEN);
                recvfrom(sockfd, &data, data_len, 0, (struct sockaddr *)&cliaddr, &len);
            }
        }
        id = atoi(strtok(data.buf, ","));
        pwd = strtok(NULL, ",");
        memset(data.buf, 0x00, MAX_MESSAGE_LEN);
        if(false == db->check_is_right(id, pwd)){
            id = -1;
        }else{
            cli_info_map[id] = cliaddr;
        }

        return id;
    }

    //很low的算法
    int get_new_port()
    {
        static int port = CHAT_PORT;
        return port++;
    }
    //发送离线数据
    void send_old_data(int recver_id)
    {/*{{{*/
        socklen_t len = sizeof(struct sockaddr);
        char *d[MAX_OLD_NUM];
        int count = 0;
        struct sockaddr_in *recver;

        recver = &cli_info_map[recver_id];
        //读取数据库
        count = db->get_data(id, d, recver_id, NOT_ONLINE);
        for(int i = 0; i < count; ++i){
            db->set_flag(data.recv_id, d[i], 1);
            memset(&data.buf, 0x00, MAX_MESSAGE_LEN);
            strtok(d[i], ",");
            d[i] = strtok(NULL, ",");
            memcpy(data.buf, d[i], strlen(d[i]));
            //发送给客户端
            sendto(sockfd, &data, data_len, 0, (struct sockaddr *)recver, len);
        }
    }/*}}}*/

private:
    int                sockfd;
    int                epollfd;//重要的fd
    char               *serip;
    int                id;
    short              port;
    data_struct        data;
    map<int, struct sockaddr_in> cli_info_map;  //存放客户端的信息
    map<int, int>      cli_fd_map; //存放sockfd， 用户发送的时候使用
    int                th_num;    //线程池里线程的个数
    struct sockaddr_in seraddr;
    struct sockaddr_in cliaddr;
    struct thread_info th_info[TH_NUM];  //创建线程的信息结构体,每一个线程拥有一个该结构体
    char               *old_data[MAX_OLD_NUM];
    int                flag[CONNECT_COUNT];
    int                data_len;
    pthread_t          cond_tid[TH_NUM];
    Msql               *db;
    pthread_mutex_t    mutex_state;
    pthread_mutex_t    mutex_que;
    queue<struct data_struct>         wait_que;
};
#endif
