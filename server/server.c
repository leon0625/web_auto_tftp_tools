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

#define _XOPEN_SOURCE 500   /* nftw 需要, 而且要写在前面 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <ftw.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "usock.h"
#include "uloop.h"
#include "easy_log.h"

#define PATH_LEN 256
#define IGNORE_CONFIG "ignore.conf"
#define SERVER_IP "0.0.0.0"
#define DEFAULT_CHECK_INTERVAL 2
#define HEART_BEAT_INTERVAL 60
#define HEART_BEAT_DATA "\x00\x01\x02\x03"

typedef struct{
    int cnt;       //文件个数
    struct stat st[];   //文件的stat信息
}file_info_t;

char ignore_conf_file[PATH_LEN] = IGNORE_CONFIG;
file_info_t *all_file;

char *g_listen_port;
char *g_path = "./";
int g_check_interval = DEFAULT_CHECK_INTERVAL;

typedef struct {
    struct uloop_fd u;
    struct sockaddr_in addr;
}client_t; 

client_t g_client;

char *strip(char *s)
{
    if(!s)
        return NULL;

    char *end = s + strlen(s);

    while(s < end)
    {
        if(*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
            break;
        *s = 0;
        s++;
    }
    --end;
    while(s < end)
    {
        if(*end != ' ' && *end != '\t' && *end != '\n' && *end != '\r')
            break;
        *end = 0;
        end--;
    }
    return s;
}

/* 从配置文件读取要过滤的目录,需要过滤返回1，否则返回0 */
int filter_with_config_file(const char *path)
{
    FILE *fp = fopen(ignore_conf_file, "r");
    char buf[256] = {0};

    if(!fp)
    {
        //printf("can't open %s\n", ignore_conf_file);
        return 0;
    }

    while(fgets(buf, sizeof(buf), fp))
    {
        strip(buf);

        /* 跳过注释行 */
        if('#' == buf[0] || '\0' == buf[0])
            continue;

        if(strstr(path, buf))
        {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

/* 过滤掉像svn的文件,需要过滤返回1，否则返回0 */
int filter(const char *path)
{
    int i = 0;
    char *key[] = {
        ".svn",
        ".o",
        ".d",
        ".ko",
        ".cmd",
        ".la",
        ".pc",
        ".lai",
        ".a",
        ".so",
        ".bin",
        ".trx"
    };

    for(i = 0; i < sizeof(key)/sizeof(char *); i++)
    {
        if(strstr(path, key[i]))
            return 1;
    }

    return filter_with_config_file(path);
}


/* nftw的回调函数，这里主要处理目录 */
int dir_file_init(const char *fpath, const struct stat *sb,
             int tflag, struct FTW *ftwbuf)
{
    struct stat st = {0};
    int cnt = 0;

    if(all_file)
        cnt = all_file->cnt;

    /* 过滤,FTW_F--普通文件 */
    if(tflag != FTW_F || filter(fpath))
        return 0;

    log_debug("init stat %s...\n", fpath);
    stat(fpath, &st);

    cnt++;
    all_file = realloc(all_file, sizeof(int) + sizeof(struct stat) * cnt);
    if(!all_file)
    {
        log_error("realloc failed, exit\n");
        exit(-1);
    }

    all_file->cnt = cnt;
    memcpy(&all_file->st[cnt-1], &st, sizeof(st));
    return 0;
}

void init_file_status(char *path)
{
    int flags = 0;
    flags |= FTW_PHYS;
    

    /* 遍历org目录，初始化文件信息 */
    if (nftw(path, dir_file_init, 100, flags) == -1) 
    {
        log_error("nftw error:%s, exit\n", strerror(errno));
        exit(1);
    }
}

void notify_file_change(const char *path)
{
    int ret = 0;
    int slen = strlen(path) + 1;

    ret = send(g_client.u.fd, path, slen, 0);
    if(ret != slen)
    {
        log_error("send %d(return %d) bytes\n", slen,  ret);
        return;
    }
    log_debug("notify [%s] modify, send %d bytes\n", path, ret);
}

/* nftw的回调函数，检查文件是否修改 */
int check_file_change(const char *fpath, const struct stat *sb,
             int tflag, struct FTW *ftwbuf)
{
    struct stat st = {0};
    int cnt = all_file->cnt;
    int i = 0;
    file_info_t *file = NULL;

    /* 过滤,FTW_F--普通文件 */
    if(tflag != FTW_F || filter(fpath))
        return 0;

    stat(fpath, &st);

    /* 先查找文件 */
    for(i = 0; i < cnt; i++)
    {
        if(st.st_ino == all_file->st[i].st_ino)
            break;
    }

    if(i == cnt)
    {
        /* 新增加的文件 */
        log_info("new file\n");
        cnt++;
        all_file = realloc(all_file, sizeof(int) + sizeof(struct stat) * cnt);
        all_file->cnt = cnt;
        memcpy(&all_file->st[cnt-1], &st, sizeof(st));
        notify_file_change(fpath);
        return 0;
    }

    /* 比较时间戳 */
    if(all_file->st[i].st_mtime != st.st_mtime)
    {
        log_info("%s has modify\n", fpath);
        memcpy(&all_file->st[i], &st, sizeof(st));
        notify_file_change(fpath);
    }

    return 0;
}

void file_check_timer_handle(struct uloop_timeout *t)
{
    int flags = 0;
    flags |= FTW_PHYS;
    
    /* 遍历org目录，初始化文件信息 */
    if (nftw(g_path, check_file_change, 100, flags) == -1) 
    {
        log_error("nftw error:%s, exit\n", strerror(errno));
        exit(1);
    }

    uloop_timeout_set(t, g_check_interval*1000);
}

void heartbeat(struct uloop_timeout *t)
{
    int ret = 0;

    ret = send(g_client.u.fd, HEART_BEAT_DATA, sizeof(HEART_BEAT_DATA), 0);
    if(ret != 4)
    {
        log_error("send %d(return %d) bytes\n", sizeof(HEART_BEAT_DATA), ret);
        return;
    }
    log_debug("send heartbeat\n");

    uloop_timeout_set(t, HEART_BEAT_INTERVAL*1000);
}

void tcp_accept_handle(struct uloop_fd *u, unsigned int events)
{
    int cli_fd = 0;
    struct sockaddr_in cli_addr = {0};
    int addr_len = sizeof(cli_addr);
    client_t *client = &g_client;
    struct uloop_fd *ufd = &client->u;

    /* 先判断是否出错 */
    if(u->error || u->eof)
    {
        log_error("(%s)\n", strerror(errno));
        exit(1);
    }

    cli_fd = accept(u->fd, (struct sockaddr *)&cli_addr, &addr_len);
    if(-1 == cli_fd)
    {
        log_error("accept error(%s)\n", strerror(errno));
        return;
    }

    log_info("client %s:%d connected\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

    memcpy(&client->addr, &cli_addr, sizeof(struct sockaddr_in));
    ufd->fd = cli_fd;

    /* 添加定时器发送心跳 */
    struct uloop_timeout *t = malloc(sizeof(struct uloop_timeout));
    t->cb = heartbeat;
    uloop_timeout_set(t, HEART_BEAT_INTERVAL*1000);
}

void usage(void)
{
    printf(
        "----------------------\n"
        "example: server -p 12345\n"
        "-p :listen port\n"
        "-P :path (default './')\n"
        "-t :check interval (default 2s)\n"
        "-i :ignore config file, default ignore.conf\n"
        "-d :debug level[%d-%d]\n", LLOG_FATAL, LLOG_VERBOSE
        );
}

int main(int argc, char *argv[])
{
    int c = -1;
    char *optstring = "hi:p:P:t:d:";
    char *path = NULL;
    struct uloop_timeout t = {0};
    struct uloop_fd ufd = {0};

    if (argc < 2)
    {
        usage();
        return 0;
    }

    while((c = getopt(argc, argv, optstring)) != -1)
    {
        switch(c)
        {
            case 'i':
                strncpy(ignore_conf_file, optarg, PATH_LEN - 1);
                break;
            case 'p':
                g_listen_port = optarg;
                break;
            case 't':
                g_check_interval = atoi(optarg);
                break;
            case 'P':
                g_path = optarg;
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

    if(!g_listen_port)
    {
        usage();
        return -1;
    }

    log_info("\nlisten port:%s\npath: %s\ncheck interval: %d\n", g_listen_port,
                g_path, g_check_interval);
    path = g_path;
    init_file_status(path);
    uloop_init();

    /* 初始化定时器 */
    t.cb = file_check_timer_handle;
    uloop_timeout_set(&t, g_check_interval*1000);

    /* 初始化socket */
    ufd.fd = usock(USOCK_TCP | USOCK_SERVER, SERVER_IP, g_listen_port);
    ufd.cb = tcp_accept_handle;
    uloop_fd_add(&ufd, ULOOP_READ);

    uloop_run(); 
    uloop_done();

    return 0;
}
