#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65536           // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 监听的最大的事件数量

// 添加文件描述符
extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd); //删除文件描述符

void addsig(int sig, void(handler)(int))
{ //添加信号捕捉，防止一个服务器断开连接，客户端继续写。
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char *argv[])
{

    if (argc <= 1)
    {
        printf("usage: %s port_number\n", basename(argv[0]));
        return 1;
    }

    int port = atoi(argv[1]);
    addsig(SIGPIPE, SIG_IGN); //捕捉信号，直接忽略 -----处理函数为SIG_IGN 忽略信号  ----------作用就是产生SIGPIPE信号时就不会中止程序，直接把这个信号忽略掉。
    // sigpipe信号详解 当服务器close一个连接时，若client端接着发数据。根据TCP协议的规定，会受到一个RST相应
    // client再往这个服务器发送数据时，系统会发出一个SIGPIPE信号给进程，告诉这个进程已经断开了

    threadpool<http_conn> *pool = NULL; //创建线程池，初始化线程池  http_conn相当于T 即treadpool里面的含有模板T的成员函数都可以使用（pool都可以使用）
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        return 1;
    }

    http_conn *users = new http_conn[MAX_FD]; // users 与http_conn共享所有属性

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    int ret = 0;
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY; //对应本地地址
    address.sin_family = AF_INET;         // tCP/IP - IPV4
    address.sin_port = htons(port);

    // 端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    ret = listen(listenfd, 5);

    // 创建epoll对象，和事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    // 添加到epoll对象中
    addfd(epollfd, listenfd, false); // false则设置EPOLLONESHOT
    http_conn::m_epollfd = epollfd;  //-----------------------------

    while (true)
    {

        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if ((number < 0) && (errno != EINTR))
        { // errno未知  EINTR跟信号有关
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {

            int sockfd = events[i].data.fd;

            if (sockfd == listenfd)
            { //有客户端连接进来

                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);

                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }

                if (http_conn::m_user_count >= MAX_FD)
                { //目前连接数满了
                    close(connfd);
                    continue;
                }
                users[connfd].init(connfd, client_address); //将新的客户的数据初始化，放到数组中
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            { //对方异常断开或者错误等事件

                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {

                if (users[sockfd].read())
                { //一次性把所有数据都读完
                    pool->append(users + sockfd);
                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {

                if (!users[sockfd].write())
                { //一次性写完所有数据
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}