#ifndef _DATABASE_H
#define _DATABASE_H

#define MAX_OLD_NUM     (99)
#define MAX_MESSAGE_LEN (1024)
#define CONNECT_COUNT      (10000)
class Msql
{
public:
    Msql()
    {}
    Msql(char *u, char *p, char *h, char *db):usr(u), passwd(p), host(h),
                                                        database(db)
    {
        mysql_init(&mys);
        id = 1000;
        if(NULL == mysql_real_connect(&mys, host, usr, passwd, database, 0, NULL, 0)){
            //cerr << "error connect mysql" << endl;
            exit(1);
        }
    }
public:
    //return login id
    int add_user_pwd(char *user, char *pwd)
    {
        char data[256] = {0};
        char buf[255] = {0};

        if(id == CONNECT_COUNT){
            return 0;
        }
        pwd = crypt(pwd, "$6");
        sprintf(data, "insert into login(id, name, passwd) values('%d', '%s', '%s');", id+1, user
                , pwd);
        //have wrong
        if(0 != mysql_query(&mys, data)){
            return 0;
        }
        //contentnum table has flag is to judge need to send data who outline
        //data content id who send and id who recv
        sprintf(buf, "create table content%d(from_id varchar(8), data varchar(%d), flag varchar(1));",id, MAX_MESSAGE_LEN);
        if(0 != mysql_query(&mys, buf)){
            return 0;
        }
        id += 1;

        return id;
    }
    void add_data(int recv_id, int flag, char *data)
    {
        char d[1100] = {0};
        sprintf(d, "insert into content%d(data, flag) values('%s', '%d');", recv_id, data, flag);
        mysql_query(&mys, d);
    }
    //return value num of messages from one id
    int get_data(int index, char *d[MAX_OLD_NUM], int id, int flag)
    {
        char data[256];
        MYSQL_ROW row;
        MYSQL_RES *result;
        int i = 0;

        mysql_init(&mys);
        mysql_real_connect(&mys, host, usr, passwd, database, 0, NULL, 0);
        sprintf(data, "select data from content%d where flag = '%d' && data like '%d%';",
                index, flag, id);
        mysql_query(&mys, data);
        if(NULL != (result = mysql_store_result(&mys))){
            while(row = mysql_fetch_row(result)){
                d[i++] = row[0];
            }
        }

        return i;
    }
    //according to id and password look login is true or false
    bool check_is_right(int id, char *pwd)
    {
        char data[256] = {0};
        char p[100] = {0};
        int res = 0;
        int flag = false;
        MYSQL_RES *result;
        MYSQL_ROW row;

        mysql_init(&mys);
        mysql_real_connect(&mys, host, usr, passwd, database, 0, NULL, 0);
        pwd = crypt(pwd, "$6");
        sprintf(data, "select * from login where id = '%d' && passwd = '%s';", id, pwd);
        res = mysql_query(&mys, data);
        result = mysql_store_result(&mys);
        row = mysql_fetch_row(result);
        if(row == NULL){
            return false;
        }else{
            return true;
        }
    }
    void set_flag(int id, char *data, int flag)
    {
        char d[256] = {0};

        sprintf(d, "update content%d set flag = %d where data='%s';", id, flag, data);
        mysql_query(&mys, d);
    }
    void change_user(char *user);
    void change_pwd(char *pwd);
private:
    MYSQL mys;
    char *usr;
    char *passwd;
    char *host;
    char *table;
    char *database;
    int  id;
};
#endif

