#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <pthread.h>

#define IP "192.168.6.32"      // 主机地址
#define IP_GROUP "224.10.10.1" // 群聊地址
#define PORT 8888              // 端口
#define PORT_GROUP 8889        // 群聊端口
#define MAXLINE 1024           // 接收数据的最大容量

int tcp_client(char *ip, int port);     // 连接服务器
int bind_udp(int sin);                  // 绑定udp套接字(群聊使用)
void printf_parse_data(char *recv_buf); // 解析收到的数据 并作出相应操作
void login_register_user();             // 登录 注册
void user_select_operation();           // 用户选择操作
void printf_user_inline_id();           // 打印在线用户id
void pravite_chat();                    // 私聊
void *pthread_chat(void *arg);          // 线程聊天
void group_chat();                      // 群聊函数
void *pthread_group_chat(void *arg);    // 线程群聊

// 使用的变量
int socket_fd;                     // 连接套接字
int udp_socket;                    // udp套接字
int epoll_fd;                      // epoll对象
struct epoll_event ev, events[10]; // epoll对象
char recv_buf[MAXLINE];            // 接收数据
char send_buf[MAXLINE];            // 发送数据
char get_buf[MAXLINE];             // 接收键盘数据
char username[20], password[20];   // 账号密码
int tid_send_to_user;

int main(int argc, char *argv[])
{
    int ret; // epoll_wait返回值(准备好的文件描述符个数)
    // 获取连接套接字
    socket_fd = tcp_client(IP, PORT);
    if (socket_fd == -1)
    {
        perror("tcp_client");
        exit(-1);
    }
    printf("连接服务器成功\n");

    // 创建epoll对象
    epoll_fd = epoll_create(1);
    if (epoll_fd == -1)
    {
        perror("epoll_create");
        exit(-1);
    }

    //
    ev.events = EPOLLIN;
    ev.data.fd = socket_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev); // 将连接套接字加入epoll对象

    // // 将读事件加入epoll对象
    // ev.events = EPOLLIN;
    // ev.data.fd = STDIN_FILENO;
    // epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev); // 将标准输入加入epoll对象

    while (1)
    {
        // 由epoll轮询
        ret = epoll_wait(epoll_fd, events, 10, -1);
        if (ret == -1)
        {
            perror("epoll_wait");
            exit(-1);
        }

        // 处理事件
        for (int i = 0; i < ret; i++)
        {
            if (events[i].data.fd == socket_fd) // 接收到的数据
            {
                memset(recv_buf, 0, sizeof(recv_buf)); // 清空recv_buf
                ret = recv(socket_fd, recv_buf, MAXLINE, 0);
                if (ret == -1)
                {
                    perror("read");
                    printf("服务器异常退出!\n");
                    exit(-1);
                }
                // printf("收到服务器消息:%s\n", recv_buf); // 测试

                printf_parse_data(recv_buf); // 解析服务器发送的指令并分析作出相应的响应

                continue;
            }
            // 接收到群聊消息<<--------------------------------------------->>--------------------

            // if (events[i].data.fd == STDIN_FILENO) // 接收到键盘数据
            // {
            //     bzero(get_buf, MAXLINE);             // 清空get_buf
            //     fgets(get_buf, MAXLINE, stdin);      // 获取键盘数据 fgets函数会将换行符一起读入
            //     get_buf[strlen(get_buf) - 1] = '\0'; // 将换行符替换为'\0'
            // }
        }
    }
}

int tcp_client(char *ip, int port) // 用用户连接服务器
{
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in sevaddr;
    sevaddr.sin_family = PF_INET;
    sevaddr.sin_port = htons(port);
    sevaddr.sin_addr.s_addr = inet_addr(ip);
    int ret = connect(sockfd, (struct sockaddr *)&sevaddr, sizeof(sevaddr));
    if (ret == -1)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void printf_parse_data(char *recv_buf) // 解析服务器发送的指令并分析作出相应的响应
{
    char sin = recv_buf[0];   // 获取指令
    printf("指令:%c\n", sin); // 测试
    switch (sin)
    {
    case '1':
        login_register_user(); // 用户执行登录注册指令
        break;
    case '2':
        printf("用户登录成功\n");
        user_select_operation(); // 用户选择操作
        break;
    case '3':
        printf("用户注册成功\n");
        login_register_user();
        break;
    case '4':
        // printf("%s\n", recv_buf + 2); // 失败返回  //*****error*****//
        printf("操作失败！\n");
        login_register_user();
        break;
    case '5':
        printf_user_inline_id(); // 打印在线用户信息
        user_select_operation();
        // sleep(2);
        break;
    case '6':

        break;
    case '7':

        break;
    case '8':

        break;
    default:
        break;
    }
}

void login_register_user() // 用户登录注册
{
    char sin; // 指令
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    // 获取指令
    while (1)
    {
        printf("-----界面-----\n");
        printf("----1.登录----\n");
        printf("----2.注册----\n");
        printf("--------------\n");
        printf("请输入指令:");
        scanf("%c", &sin);
        getchar();
        if (sin == '1' || sin == '2')
        {
            break;
        }
        printf("指令错误，重新输入指令\n");
    }

    // 获取账号密码
    printf("请输入账号:");
    fgets(username, 20, stdin);
    username[strlen(username) - 1] = '\0'; // 将换行符替换为'\0'
    printf("请输入密码:");
    fgets(password, 20, stdin);
    password[strlen(password) - 1] = '\0'; // 将换行符替换为'\0'

    // 拼接要发送的数据
    memset(send_buf, 0, MAXLINE); // 清空send_buf
    sprintf(send_buf, "%c %s %s", sin, username, password);

    printf("发送给服务器的消息:%s\n", send_buf); // 测试

    // 将指令和账号密码发送给服务器
    send(socket_fd, send_buf, MAXLINE, 0);
}

void user_select_operation() // 用户选择操作
{
    char sin;            // 指令
    char buf[512] = {0}; // 额外数据
    while (1)
    {
        memset(&sin, 0, sizeof(sin));
        printf("---------------------\n");
        printf("------欢迎使用-------\n");
        printf("---------------------\n");
        printf("---3.查看在线用户----\n");
        printf("---4.私聊------------\n");
        printf("---5.群聊------------\n");
        printf("---6.退出------------\n");
        printf("---------------------\n");

        printf("请输入指令:");
        scanf("%c", &sin);
        getchar();
        if (sin == '3' || sin == '4' || sin == '5' || sin == '6')
        {
            break;
        }
        printf("指令错误，重新输入指令\n");
    }
    memset(send_buf, 0, MAXLINE); // 清空send_buf
    if (sin == '4')
    {
        pravite_chat(); // 私聊函数
        user_select_operation();
        return;
    }
    else if (sin == '5')
    {
        group_chat(); // 群聊函数
        user_select_operation();
        return;
    }
    else if (sin == '6')
    {
        close(socket_fd);
        exit(0);
    }
    else
    {
        // 拼接要发送的数据
        sprintf(send_buf, "%c", sin); // 将指令放入send_buf
    }
    // 发送指令给服务器
    send(socket_fd, send_buf, MAXLINE, 0);
    printf("发送给服务器的消息:%s\n", send_buf); // 测试
}

void printf_user_inline_id()
{
    char sin;
    char buf[MAXLINE] = {0};
    printf("在线用户的id：");
    while (1)
    {
        memset(recv_buf, 0, MAXLINE);          // 清空recv_buf
        recv(socket_fd, recv_buf, MAXLINE, 0); // 接收服务器发送的数据
        // 拆分信息
        sscanf(recv_buf, "%c %s", &sin, buf);

        // 分析指令作出响应
        if (recv_buf[0] == '0')
        {
            break;
        }

        printf("%s ", buf); // 打印在线用户信息
    }
    printf("\n");
}

void pravite_chat() //
{
    char change_recv_id[20];
    char buf[512];  // 消息头部
    char info[512]; // 消息内容
    pthread_t tid;  // 线程id

    printf("请输入要私聊的用户id:");
    fgets(change_recv_id, 20, stdin);
    change_recv_id[strlen(change_recv_id) - 1] = '\0'; // 将换行符替换为'\0'
    sprintf(buf, "%s %s", "4", change_recv_id);        // 将指令和用户id放入send_buf 得到前端消息

    // 发送开始指令给服务器
    sprintf(send_buf, "%s %s", buf, "start");
    send(socket_fd, send_buf, MAXLINE, 0);

    // 开线程接收服务器发来的私人消息
    pthread_create(&tid, NULL, pthread_chat, NULL);

    while (1) // 获取消息内容
    {
        printf("tip:输入exit退出，输入change改变私聊对象 接收方为:%s\n", change_recv_id);
        // printf("接收方:%s内容:", change_recv_id);

        memset(info, 0, 512);          // 清空info
        fgets(info, 512, stdin);       // 获取用户输入
        info[strlen(info) - 1] = '\0'; // 将换行符替换为'\0'

        // 判断是否要退出
        if (strcmp(info, "exit") == 0)
        {
            //  退出私聊
            pthread_cancel(tid); // 取消接收私聊消息线程
            // 拼接消息
            memset(&send_buf, 0, MAXLINE);                        // 清空send_buf
            sprintf(send_buf, "%s %s %s", "4", "线程号", "exit"); // 将指令和用户id放入send_buf 得到前端消息
            send(socket_fd, send_buf, MAXLINE, 0);                // 发送取消指令
            return;
        }

        // 判断是否要改变私聊对象
        if (strcmp(info, "change") == 0)
        {
            memset(&change_recv_id, 0, sizeof(change_recv_id));
            printf("请输入要私聊的用户id:");
            fgets(change_recv_id, 20, stdin);
            change_recv_id[strlen(change_recv_id) - 1] = '\0'; // 将换行符替换为'\0'
            sprintf(buf, "%s %s", "4", change_recv_id);        // 将指令和用户id放入send_buf 得到头部消息
            continue;
        }

        // 拼接消息
        sprintf(send_buf, "%s %s", buf, info); // 将头部消息和消息内容放入send_buf 得到消息

        // 发送消息给服务器
        send(socket_fd, send_buf, MAXLINE, 0);
    }
}

void *pthread_chat(void *arg) // 接收服务器发来的私人消息
{
    while (1)
    {
        memset(recv_buf, 0, MAXLINE);          // 清空recv_buf
        recv(socket_fd, recv_buf, MAXLINE, 0); // 接收服务器发送的数据
        printf("%s\n", recv_buf);              // 打印接收到的消息
    }
}

void group_chat() // 群聊
{
    udp_socket = bind_udp(socket_fd); // 客户机 进入 udp 模式 接收群发消息

    // 开线程接收群发消息
    pthread_t tid;
    pthread_create(&tid, NULL, pthread_group_chat, NULL);

    // 发送进入群聊指令给服务器
    sprintf(send_buf, "%s %s", "5", "start");
    send(socket_fd, send_buf, MAXLINE, 0);

    while (1) // 接收键盘消息发送给服务器
    {
        memset(recv_buf, 0, MAXLINE);          // 清空recv_buf
        fgets(recv_buf, MAXLINE, stdin);       // 获取用户输入
        recv_buf[strlen(recv_buf) - 1] = '\0'; // 将换行符替换为'\0'

        // 判断是否要退出
        if (strcmp(recv_buf, "exit") == 0)
        {
            //  退出群聊 关闭udp套接字 并移除epoll对象

            memset(send_buf, 0, MAXLINE);          // 清空send_buf
            sprintf(send_buf, "%s", "exit");       // 将指令和用户id放入send_buf 得到前端消息
            send(socket_fd, send_buf, MAXLINE, 0); // 发送取消指令
            printf("退出群聊\n");
            close(udp_socket);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, udp_socket, NULL);
            return;
        }

        // 拼接消息
        memset(send_buf, 0, MAXLINE);
        sprintf(send_buf, "%s", recv_buf);     // 将指令和消息内容放入send_buf 得到消息
        send(socket_fd, send_buf, MAXLINE, 0); // 发送消息给服务器
    }
}

int bind_udp(int sin) // 客户机 进入 udp 模式 接收群发消息
{
    int sockfd;
    struct sockaddr_in servaddr;

    char buf[128];
    int ret;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0); // 创建套接字(UDP)
    if (sockfd < 0)
    {
        perror("socket");
        exit(-1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT_GROUP);     // 通过用户套接字 确定端口
    servaddr.sin_addr.s_addr = inet_addr("0"); // 0地址代表本机地址

    ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret < 0)
    {
        perror("bind");
        exit(-1);
    }

    struct ip_mreqn
    {
        struct in_addr imr_multiaddr; /* IP multicast group  address */
        struct in_addr imr_address;   /* IP address of local interface */
        int imr_ifindex;              /* interface index */
    };

    struct ip_mreqn imr;
    bzero(&imr, sizeof(imr));
    imr.imr_multiaddr.s_addr = inet_addr(IP_GROUP); // 组播地址
    imr.imr_address.s_addr = inet_addr("0.0.0.0");  // 本机地址

    // 通过setsockopt 设置套接字属性 （加入多播组）
    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr));

    return sockfd;
}

void *pthread_group_chat(void *arg) // 接收服务器发来的群发消息
{
    while (1)
    {
        memset(recv_buf, 0, MAXLINE);                     // 清空recv_buf
        recvfrom(udp_socket, recv_buf, MAXLINE, 0, 0, 0); // 接收服务器发送的数据
        printf("%s\n", recv_buf);                         // 打印接收到的消息
    }
}
