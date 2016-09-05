#pragma once
#include <vector>
#include <map>
#include <iostream>
using namespace std;
//遇到的问题：
//1.把中间跳过的包的id都存放一个容器，不断轮巡请求重发，然后等到接受到id就删除容器里的id
//但是如何找这个id效率比较高
//2.重发的数据在路上，发送端继续收到重发标记，又一次重发该数据，导致数据重复发送，浪费效率

//解决
//1。弄一个链表，把需要重发的id全都挂到链表上。再弄一个map，map[id]=1,如果发送的id对应的map
//值是1，就去链表里找那个id，并且删除它



//发送方：
//所有发的数据放到缓冲区中
//等到对应的ack，删除缓冲里对应的元素
//设置一个超时时间，如果没有等到ack，但是中途有其他ack（说明没断网），那么就自动删除（防止ack丢失）
//接收重传标记，那么就重传对应数据包

//接受方：
//接收id，如果有跳跃，记录跳跃的syn，等待一段时间，如果对应的包发过来，就删除记录表对应的元素
//如果没有发过来，不断的重复的发送对应包syn请求，直到发过来包
//接受到的包的syn，都去发送对应的ack

//recv:
//等到对应的ack，删除缓冲里对应的元素
//接收重传标记，那么就重传对应数据包
//接收id，如果有跳跃，记录跳跃的syn，等待一段时间，如果对应的包发过来，就删除记录表对应的元素
//接受到的包的syn，都去发送对应的ack
//如果没有发过来，发送对应包syn请求
//设置一个超时时间，如果没有等到ack，但是中途有其他ack（说明没断网），那么就自动删除（防止ack丢失）

//send:
//所有发的数据放到缓冲区中

//加一个线程，专门请求未收到的包

map <int, data_struct> m;
vector<int>            v;
int value = 0;
struct Info
{
    int sockfd;
    int flags;
    struct sockaddr_in addr;
};

ssize_t send_f(int sockfd, void *buf, size_t size, int flags, const struct sockaddr *addr, socklen_t len)
{
    data_struct d;
    ssize_t sz;
    static int id = rand();

    //存放到map中
    memset(&d, 0x00, sizeof(d));
    memcpy(&d, buf, sizeof(d));
    d.syn_id = id;
    ++d.syn_id;
    d.ack_flag = 0;
    d.ack_id = 0;
    m[d.syn_id] = d;
    sz = sendto(sockfd, buf, size, flags, addr, len);

    return sz;
}
//m是存放发送过包数据的容器，防止未发送到。v是存放重发syn_id的容器。value是上一次发送数据的syn_id

//收到确认的包，有两种情况，一种是收到过确认并且把包删除了；另一种是第一次收到该确认包
//那么就需要把发过包的缓冲m删除，还需要找看是否是之前重发的包，把v中的删除

//收到了重发标记，就去发一个对应的包

//如果value值和syn_id的值相差大于1，那么说明中途有包丢失，把中途丢失包的id存到v中
ssize_t recv_f(int sockfd, void *data, size_t size, int flags, struct sockaddr *addr, socklen_t *len)
{
    data_struct buf;
    int count = 1;
    vector<int> :: iterator it;

    memset(data, 0x00, sizeof(data));
    memset(&buf, 0x00, sizeof(data));
    recvfrom(sockfd, &buf, size, flags, addr, len);
    //是确认的包,删除缓冲里对应的元素
    if(buf.ack_flag == 1){
        //删除
        it = v.begin();
        while(it != v.end() && *it != buf.ack_id-1){
            ++it;
        }
        if(it == v.end()){
            //已经删除了
        }else{
            v.erase(it);
            m.erase(buf.ack_id-1);
        }
    }else if(buf.again_flag == 1 && &m[buf.syn_id] != NULL){
        //接收到重发标记，并且没有发到
        buf.again_flag = 0;
        sendto(sockfd, &m[buf.syn_id], size, flags, addr, *len);
    }
    if(value == 0)
        value = buf.syn_id-1;
    else if(value+1 != buf.syn_id){
        //说明中途有包失踪
        while(value+1 < buf.syn_id){
            //放入容器
            ++value;
            v.push_back(value);
        }
    }
    ++value;
    //发送确认
    memset(data, 0x00, sizeof(data));
    ((data_struct *)data)->ack_flag = 1;
    ((data_struct *)data)->ack_id = value+1;
    sendto(sockfd, data, size, flags, addr, *len);
}

//对于中途没有收到的，轮巡发送请求
void *send_thread(void *arg)
{
    struct Info info;
    socklen_t len = sizeof(struct sockaddr);
    int size;
    vector<int> :: iterator it;

    memcpy(&info, arg, sizeof(info));
    for(;;){
        for(it = v.begin(); it != v.end(); ++it){
            size = sizeof(m[*it]);
            //如果为空，说明已经发送过
            if(&m[*it] == 0){
                continue;
            }
            m[*it].ack_id = 0;
            m[*it].again_flag = 1;
            sendto(info.sockfd, &m[*it], size, info.flags, (const struct sockaddr *)&(info.addr), len);
        }
        //需要睡眠一段时间，为了保证这段时间内可以接受到ack，并且删除v中对应的值
        sleep(3);
    }
}
