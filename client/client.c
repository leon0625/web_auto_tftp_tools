/************************************************************
Copyright (C), 2019, Leon, All Rights Reserved.

      ██╗     ███████╗ ██████╗ ███╗   ██╗
      ██║     ██╔════╝██╔═══██╗████╗  ██║
      ██║     █████╗  ██║   ██║██╔██╗ ██║
      ██║     ██╔══╝  ██║   ██║██║╚██╗██║
      ███████╗███████╗╚██████╔╝██║ ╚████║
      ╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═══╝                                         
Description: 检测到文件修改自动tftp工具
Author: Leon
Version: 1.0
Date: 
************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stddef.h> /* offsetof */
#include <errno.h>

#include "uloop.h"
#include "usock.h"
#include "easy_log.h"

#define HEART_BEAT_DATA "\x00\x01\x02\x03"

char *g_connect_ip, *g_connect_port, *g_tftp_server_ip;

void tcp_recv_handle(struct uloop_fd *u, unsigned int events)
{
    char path[1024] = {0};
    char cmd[1024] = {0};
    int len = 0;

    /* 先判断是否出错 */
    if(u->error)
    {
        log_debug("delete fd\n");
        goto exit;
    }

    /* 接收数据 */
    len = recv(u->fd, path, sizeof(path) - 1, 0);
    if(len <= 0)
    {
        log_warn("recv 0 bytes, exit\n");
        goto exit;
    }

    log_verbose("recv %d bytes\n", len);

    if(!memcmp(path, HEART_BEAT_DATA, sizeof(HEART_BEAT_DATA)))
    {
        log_debug("recv heartbeat\n");
    }
    else
    {
        snprintf(cmd, sizeof(cmd), "tftp -gr '%s' %s -l '%s'", path, g_tftp_server_ip, path);
        log_info("[%s]\n", cmd);
        system(cmd);
    }

    if(u->eof)
    {
        log_debug("close socket\n");
        goto exit;
    }
    return;
    
exit:
    uloop_fd_delete(u);
    close(u->fd);
    exit(1);
}

void usage(void)
{
    printf(
        "----------------------\n"
        "example: client -c 192.168.178.4 -p 12345 -t 192.168.10.123\n"
        "windows add port forward: netsh interface portproxy add v4tov4 listenport=12345 listenaddress=0.0.0.0 connectport=12345 connectaddress=192.168.178.4"
        "-c :connect ip, file monitor server ip\n"
        "-p :connect port, file monitor server port\n"
        "-t :tftp server ip\n"
        "-d :debug level[%d-%d]\n", LLOG_FATAL, LLOG_VERBOSE
        );
}

int main(int argc, char *argv[])
{
    char *optstring = "hp:c:t:d:";
    struct uloop_fd ufd = {0};
    int c = -1;

    if (argc < 2)
    {
        usage();
        return 0;
    }

    while((c = getopt(argc, argv, optstring)) != -1)
    {
        switch(c)
        {
            case 'p':
                g_connect_port = optarg;
                log_debug("port is %s\n", g_connect_port);
                break;
            case 'c':
                g_connect_ip = optarg;
                log_debug("file monitor server ip is %s\n", g_connect_ip);
                break;
            case 't':
                g_tftp_server_ip = optarg;
                log_debug("tftp server ip is %s\n", g_tftp_server_ip);
                break;
            case 'd':
                log_set_level(atoi(optarg));
                break;
            case 'h':
            default:
                usage();
                return -1;
        }
    }

    if(!g_connect_port || !g_connect_ip || !g_tftp_server_ip)
    {
        usage();
        return -1;
    }

    uloop_init();

    ufd.fd = usock(USOCK_TCP, g_connect_ip, g_connect_port);
    if(-1 == ufd.fd)
    {
        log_error("connect failed, %s, exit\n", strerror(errno));
        exit(1);
    }
    log_info("connect %s:%s success\n", g_connect_ip, g_connect_port);
    ufd.cb = tcp_recv_handle;
    uloop_fd_add(&ufd, ULOOP_READ);

    /* 框架完成 */
    uloop_run();
    uloop_done();
    return 0;
}
