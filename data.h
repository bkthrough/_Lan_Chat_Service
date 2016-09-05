#ifndef _DATA_H
#define _DATA_H

#define MAX_MESSAGE_LEN (1024)
#define MAX_ID_NUM      (100)
struct data_struct
{
    char  buf[MAX_MESSAGE_LEN];
    int   chat_port;//聊天使用的端口，在认证之后发送给客户端
    int   begin_flag;
    int   end_flag;
    int   send_id;//发送端的id
    int   recv_id;//接受端的id
    int   newc_flag;
    union{
        int   new_id_flag;
        int   new_id;
        int   login_flag;
    };
    int   group_flag;
    int   syn_id;
    int   ack_id;
    char  ack_flag;
    char  again_flag;
    int   group_id;
    int   not_read_id[MAX_ID_NUM];
};

#endif
