#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/epoll.h>
#include <sqlite3.h>

#define IP "0"                 // 服务器ip
#define IP_GROUP "224.10.10.1" // 群聊地址
#define PORT 8888              // 服务器端口
#define PORT_GROUP 8889        // 群聊端口
#define SIZE 1024              // 数据收发的统一大小

typedef struct
{
    long mtype;       // 消息类型
    char mtext[SIZE]; // 消息内容
} msgbuf;

/////
char recv_buf[SIZE];       // 接收缓冲区
char recv_buf_group[SIZE]; // 接收缓冲区
char send_buf[SIZE];       // 发送缓冲区
char send_buf_group[SIZE]; // 发送缓冲区
char recv_sin;             // 接收到的指令
char send_sin;             // 发送的指令
int epollfd;               // epoll实例
int epollfd2;              // epoll实例2

const char *db_name = "user.db"; // 数据库名称
/////
// 回调函数
int callback(void *data, int argc, char **argv, char **azColName)
{
    int *flag = (int *)data;
    *flag = atoi(argv[2]); // 表示数据库有匹配数据 登录成功 返回一个非零值(id)
    // printf("%d\n", *flag);
    return 0;
}

int callback2(void *data, int argc, char **argv, char **azColName)
{
    int connfd = *(int *)data;

    // 准备发送给客户端的信息
    send_sin = '5';
    sprintf(send_buf, "%c %s", send_sin, argv[0]);

    // 发送信息给客户端
    send(connfd, send_buf, sizeof(send_buf), 0);

    return 0;
}

int callback3(void *data, int argc, char **argv, char **azColName)
{
    int *id = (int *)data;
    *id = atoi(argv[0]);
    return 0;
}

/////
int tcp_server(char *ip, int port);      // 服务器服务端
void login_user(int connfd);             // 登录
void user_on_line(int sockfd, int id);   // 用户上线
void user_off_line(int sockfd, int id);  // 用户下线 id未使用
void register_user(int connfd);          // 注册
void check_user(int connfd);             // 查看在线用户
void private_chat(int connfd);           // 私聊
void *private_char_send_msg(void *data); // 服务器转发私聊消息
int get_msg();                           // 获取消息队列id
int get_id(int sockfd);                  // 通过通信套接字 获取 id
void group_chat(int connfd);             // 群聊
void *group_message(void *arg);          // 群聊线程

int main(int argc, char *argv[])
{
    ////
    int sockfd;                       // 监听套接字
    int connfd;                       // 连接套接字
    struct epoll_event ev, event[10]; // epoll事件
    int ret;                          // epoll事件个数
    int flag;                         // 接收返回值
    struct sockaddr_in client_addr;   // 客户端地址
    socklen_t len = sizeof(client_addr);
    ////
    sockfd = tcp_server(IP, PORT);
    if (-1 == sockfd)
    {
        return -1;
    }
    printf("服务器启动成功\n");

    epollfd = epoll_create(1); // 创建epoll实例
    if (-1 == epollfd)
    {
        perror("epoll_create");
        return -1;
    }

    ev.events = EPOLLIN;                            // 设置事件类型
    ev.data.fd = sockfd;                            // 设置事件数据
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev); // 将监听套接字加入epoll实例

    epollfd2 = epoll_create(1); // 创建epoll实例2 该轮询用于接收群聊消息并转发给所有加入群聊的用户
    if (-1 == epollfd2)
    {
        perror("epoll_create");
        return -1;
    }
    // 创建群聊线程
    pthread_t tid_group;
    pthread_create(&tid_group, NULL, group_message, NULL);

    printf("等待客户端连接\n");
    while (1)
    {

        ret = epoll_wait(epollfd, event, 10, -1);
        if (ret < 0)
        {
            perror("epoll_wait");
            exit(-1);
        }

        for (int i = 0; i < ret; i++) // 遍历事件
        {
            if (event[i].data.fd == sockfd) // 如果是监听套接字
            {
                connfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
                if (-1 == connfd)
                {
                    perror("accept");
                    exit(-1);
                }

                ev.events = EPOLLIN;                            // 设置事件类型
                ev.data.fd = connfd;                            // 设置事件数据
                epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev); // 将连接套接字加入epoll实例
                char a = '1';
                send(connfd, &a, sizeof(a), 0);
                // printf("客户端:%d连接成功\n");
                printf("等待客户端连接\n");
            }
            else // if (event[i].data.fd == connfd) // 接收客户端数据
            {
                // 接收到的数据
                connfd = event[i].data.fd;
                memset(recv_buf, 0, SIZE);
                flag = recv(connfd, recv_buf, SIZE, 0);
                if (flag <= 0)
                {
                    printf("客户端断开连接\n");
                    user_off_line(connfd, connfd);                   // 用户下线
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, NULL); // 踢出轮询
                    close(connfd);
                    continue;
                }
                printf("客户端:%d发送数据:%s\n", connfd, recv_buf); // 测试
                // 解析数据 提取命令
                recv_sin = recv_buf[0];

                switch (recv_sin)
                {
                case '1': // 执行登录
                    login_user(connfd);
                    break;
                case '2': // 执行注册
                    register_user(connfd);
                    break;
                case '3': // 执行查看在线人
                    check_user(connfd);
                    break;
                case '4': // 执行私聊
                    private_chat(connfd);
                    break;
                case '5': // 执行群聊
                    // printf("等待完成\n");
                    group_chat(connfd);
                    break;
                case '6': // 执行退出
                    close(connfd);
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, NULL); // 踢出轮询
                default:
                    break;
                }
            }
        }
    }
}

int tcp_server(char *ip, int port) // 开启Tcp服务
{
    // 套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket");
        return -1;
    }

    // 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    // 设置可重用
    int on = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (-1 == ret)
    {
        perror("setsockopt");
        return -1;
    }

    ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (-1 == ret)
    {
        perror("bind");
        return -1;
    }

    // 监听
    listen(sockfd, 5);

    return sockfd;
}

void login_user(int connfd) // 登录
{
    char password[20] = {0};
    char account[20] = {0};
    char *err_msg;
    char SQL[512];
    int id = 0;
    int flag = 0; // 用与记录是否登录成功
    int ret;
    sqlite3 *db;
    // 将数据包中的账号密码提取出来
    strcpy(recv_buf, recv_buf + 2);               // 覆盖指令
    sscanf(recv_buf, "%s %s", account, password); // 获得账号密码

    // 打开数据库
    ret = sqlite3_open(db_name, &db);
    if (ret != SQLITE_OK)
    {
        printf("打开数据库失败\n");
        return;
    }

    // 构建SQL语句
    sprintf(SQL, "SELECT * FROM user WHERE account='%s' AND password='%s';", account, password);
    // printf("%s\n", SQL);

    //  执行SQL语句
    ret = sqlite3_exec(db, SQL, callback, (void *)&flag, &err_msg);
    if (ret != SQLITE_OK)
    {
        printf("error:%s\n", err_msg);
        return;
    }

    // 关闭数据库
    sqlite3_close(db);

    // 分析返回的flag
    send_sin = flag > 0 ? '2' : '4'; // 登录成功返回2 失败返回4
    id = flag;                       // 发送id给登录成功后的用户

    // 拼接回传给用户的信息
    bzero(send_buf, sizeof(send_buf));           // 清空发送缓冲区
    sprintf(send_buf, "%c", send_sin);           // 指令
    send(connfd, send_buf, strlen(send_buf), 0); // 发送回传信息

    // printf("%s\n", send_buf); // 测试

    if (flag > 0)
    {
        user_on_line(connfd, id); // 上线
    }
}

void user_on_line(int sockfd, int id) // 上线
{
    int falg = 0;
    sqlite3 *db;
    int ret;
    char sql[256];

    // 打开数据库
    ret = sqlite3_open(db_name, &db);
    if (ret != SQLITE_OK)
    {
        printf("打开数据库失败\n");
        return;
    }
    // 如过没有表就创建
    strcpy(sql, "CREATE TABLE IF NOT EXISTS data_on_line (id int PRIMARY KEY, socked_fd int);");
    ret = sqlite3_exec(db, sql, NULL, NULL, NULL);

    // 构建SQL语句
    char SQL[512];

    sprintf(SQL, "INSERT INTO data_on_line VALUES (%d,%d);", id, sockfd);

    // 执行SQL语句
    ret = sqlite3_exec(db, SQL, NULL, NULL, NULL);
    if (ret != SQLITE_OK)
    {
        printf("上线失败\n");
    }

    // 关闭数据库
    sqlite3_close(db);
}

void user_off_line(int sockfd, int id) // 离线 id未使用
{
    sqlite3 *db;
    int ret;
    char *err_msg;
    char SQL[512];

    // 打开数据库
    ret = sqlite3_open(db_name, &db);
    if (ret != SQLITE_OK)
    {
        printf("打开数据库失败\n");
        return;
    }

    // 构建SQL语句
    sprintf(SQL, "delete from data_on_line where socked_fd = %d;", sockfd);
    // printf("SQL删除语句：%s\n", SQL);

    // 执行SQL语句
    ret = sqlite3_exec(db, SQL, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
    {
        printf("离线出错:%s\n", err_msg);
        return;
    }

    // 关闭数据库
    sqlite3_close(db);
}

void register_user(int connfd) // 注册
{
    char *err_msg;
    char sin;
    char password[20] = {0};
    char account[20] = {0};
    char SQL[512];
    int flag = 1; // 用与记录是否注册成功
    int ret;
    sqlite3 *db;

    // 将数据包中的账号密码提取出来
    // strcpy(recv_buf, recv_buf + 2);               // 覆盖指令
    sscanf(recv_buf, "%c %s %s", &sin, account, password); // 获得账号密码

    // 打开数据库
    ret = sqlite3_open(db_name, &db);
    if (ret != SQLITE_OK)
    {
        printf("打开数据库失败\n");
        return;
    }

    // 如过没有表就创建
    char sql[256] = "CREATE TABLE IF NOT EXISTS user (account char UNIQUE, password char, id INTEGER PRIMARY KEY);";
    ret = sqlite3_exec(db, sql, NULL, NULL, NULL);

    // 构建SQL语句
    sprintf(SQL, "insert into user  (account,password) values ('%s', '%s');", account, password);

    // 执行SQL语句
    ret = sqlite3_exec(db, SQL, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
    {
        printf("注册失败%s\n", err_msg);
        flag = 0;
    }

    // 关闭数据库
    sqlite3_close(db);

    // 拼接数据
    send_sin = flag > 0 ? '3' : '4';   // 注册成功返回3
    sprintf(send_buf, "%c", send_sin); // 指令
    printf("%s\n", send_buf);          // 测试

    // 回复客户端
    send(connfd, send_buf, strlen(send_buf), 0); // 发送回传信息
}

void check_user(int connfd)
{

    int falg = 0;
    sqlite3 *db;
    int ret;
    char SQL[512];
    char *err_msg;
    // 打开数据库
    ret = sqlite3_open(db_name, &db);
    if (ret != SQLITE_OK)
    {
        printf("打开数据库失败\n");
        return;
    }

    // 构建SQL语句
    sprintf(SQL, "select * from data_on_line;");

    send_sin = '5';
    sprintf(send_buf, "%c", send_sin);           // 开始查看指令
    send(connfd, send_buf, strlen(send_buf), 0); // 发送回传信息
    printf("服务器发送了指令：%s\n", send_buf);  // 测试

    // 执行SQL语句
    ret = sqlite3_exec(db, SQL, callback2, (void *)&connfd, &err_msg);
    if (ret != SQLITE_OK)
    {
        printf("查看在线表失败 :%s\n", err_msg);
    }

    // 关闭数据库
    sqlite3_close(db);

    // 结束查看
    send_sin = '0';
    sprintf(send_buf, "%c", send_sin);           // 结束指令
    send(connfd, send_buf, strlen(send_buf), 0); // 发送回传信息
}

void private_chat(int connfd)
{
    char sin[20];
    char id[20] = {0}, buf[SIZE] = {0};
    int recv_id, send_id;
    int msgid = get_msg(); // 消息队列号
    int len;

    msgbuf msg;
    memset(&msg, 0, sizeof(msg.mtext)); // 清空消息

    // 获取接收方id 和消息内容
    sscanf(recv_buf, "%s %s %s", sin, id, buf); // 指令 接收方id 消息内容
    recv_id = atoi(id);                         // 接收方id
    msg.mtype = recv_id;                        // 消息类型
    send_id = get_id(connfd);                   // 发送方id
    len = strlen(sin) + strlen(id) + 2;         // 定位消息内容的起始位置

    // 如果接收到开启指令 开启一个线程向客户端发送消息
    if (strcmp(buf, "start") == 0)
    {
        // 开启线程发送消息给客户端
        pthread_t tid;
        pthread_create(&tid, NULL, private_char_send_msg, (void *)&connfd);

        printf("开启线程\n");
        return;
    }

    // 如果接收到取消指令关闭发送消息的线程
    if (strcmp(buf, "exit") == 0)
    {
        // 关闭一个向客户端发送消息的线程 //------>>向消息队列给向断开连接的用户发送一个结束消息<<------------------

        // 拼接结束消息
        sprintf(msg.mtext, "%s", "exit");
        msg.mtype = send_id;                       // 消息类型(给自己发送一个消息，但这条消息不会被客户端接收)
        msgsnd(msgid, &msg, sizeof(msg.mtext), 0); // 线程接收到这条消息后会立即结束

        printf("关闭线程\n");
        return;
    }

    // 拼接消息内容
    sprintf(msg.mtext, "%d:", send_id);
    strcat(msg.mtext, recv_buf + len); // 发送人id:内容

    // 发送消息 到消息队列
    printf("发送到消息队列的消息%s,消息类型为%ld\n", msg.mtext, msg.mtype);
    msgsnd(msgid, &msg, sizeof(msg.mtext), 0);
}

int get_msg() // 获取消息队列号
{
    key_t key = ftok(".", '2');
    if (-1 == key)
    {
        perror("ftok");
        return -1;
    }

    int msgid = msgget(key, 0664 | IPC_CREAT);
    if (-1 == msgid)
    {
        perror("msgget");
        return -1;
    }

    return msgid;
}

int get_id(int sockfd) // 通过通信套接字 获取 id
{
    int id = 0;
    // 打开数据库(user.db)
    sqlite3 *db;
    int ret = sqlite3_open(db_name, &db);
    if (ret != SQLITE_OK)
    {
        printf("打开数据库失败\n");
        return 0;
    }

    // 构造sql语句
    char SQL[512];
    sprintf(SQL, "SELECT * FROM data_on_line WHERE socked_fd='%d';", sockfd);
    // printf("SQL语句：%s\n", SQL);

    // 执行sql语句
    ret = sqlite3_exec(db, SQL, callback3, (void *)&id, NULL);
    if (ret != SQLITE_OK)
    {
        printf("查询失败\n");
        // return 0;
    }

    // 关闭数据库
    sqlite3_close(db);
    return id;
}

void *private_char_send_msg(void *arg)
{
    int connfd = *(int *)arg;
    int msg_get_id;
    int msgid = get_msg(); // 获取消息队列号
    // 开启线程向客户端发送消息
    pthread_detach(pthread_self());

    while (1)
    {
        msg_get_id = get_id(connfd); // 获取当前客户端的id
        if (msg_get_id == 0)         // 用户异常离线
        {
            printf("线程结束,用户异常退出\n");
            pthread_exit(NULL);
        }

        msgbuf msg;
        msgrcv(msgid, &msg, sizeof(msg.mtext), msg_get_id, 0); // 从消息队列中获取消息

        if (strcmp(msg.mtext, "exit") == 0)
        {
            printf("线程结束\n");
            pthread_exit(NULL);
        }

        printf("线程接收到的消息：%s\n", msg.mtext);
        strcpy(send_buf, msg.mtext);
        send(connfd, send_buf, strlen(send_buf), 0); // 发送消息给客户端
    }
    return NULL;
}

void group_chat(int connfd) // 群聊
{
    // 将套接字脱离epoll加入epoll2(群聊轮询)
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = connfd;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, NULL);

    epoll_ctl(epollfd2, EPOLL_CTL_ADD, connfd, &ev);
}

void *group_message(void *arg) // epoll2 群发消息
{
    int ret, bytes;
    struct epoll_event events[10];

    pthread_detach(pthread_self());
    struct epoll_event ev;
    ev.events = EPOLLIN;
    int id;

    // 群聊 组播
    // 创建udp套接字
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        // exit(1);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT_GROUP);
    servaddr.sin_addr.s_addr = inet_addr(IP_GROUP);

    while (1)
    {
        ret = epoll_wait(epollfd2, events, 128, -1);
        if (ret == -1)
        {
            perror("epoll_wait");
            exit(1);
        }

        for (int i = 0; i < ret; i++)
        {
            int connfd = events[i].data.fd;
            id = get_id(connfd); // 获取用户id
            // 接收消息tcp
            memset(recv_buf_group, 0, sizeof(recv_buf_group));
            bytes = recv(connfd, recv_buf_group, sizeof(recv_buf_group), 0);
            if (bytes <= 0)
            {
                printf("client %d exit\n", connfd);
                user_off_line(connfd, id);
                epoll_ctl(epollfd2, EPOLL_CTL_DEL, connfd, NULL);
                close(connfd);
                continue;
            }
            // 接收到的消息
            printf("接收到的消息:%s\n", recv_buf_group);

            // 接收到 退出群聊命令
            if (strcmp(recv_buf_group, "exit") == 0)
            {
                // 离开当前 epoll2 回到 eopll
                printf("用户%d退出群聊\n", id);
                epoll_ctl(epollfd2, EPOLL_CTL_DEL, connfd, NULL);
                ev.data.fd = connfd;
                ev.events = EPOLLIN;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev);
                continue;
            }

            // 将用户信息id拼接到消息前面
            memset(send_buf_group, 0, sizeof(send_buf_group));
            sprintf(send_buf_group, "用户%d:", id);
            strcat(send_buf_group, recv_buf_group);

            // 广播消息 udp
            printf("群发消息:%s\n", send_buf_group);
            sendto(sockfd, send_buf_group, strlen(send_buf_group), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        }
    }
}
