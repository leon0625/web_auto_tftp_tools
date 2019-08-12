/************************************************************
Copyright (C), 2018, Leon, All Rights Reserved.
FileName: easy_log_extend.c
Description: 实现日志配置文件的解析
Author: Leon
Version: 1.0
Date: 
Function:

History:
<author>    <time>  <version>   <description>
 Leon
************************************************************/

/*********************************************************** 
  大体思路：
  (1) 每个进程对应的日志配置文件默认在/var/log_conf/xxx_log.conf下
  (2) 如果没有这个文件，那么日志等级默认为info等级
  (3) 可以设置日志文件最大大小

配置文件使用：
level=3     #日志等级
file=/var/123.log   #日志是否保存到文件，不保存到文件没有这行
size=100k       # 日志文件大小限制，单位KB，可以没有
 ***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "easy_log.h"

pthread_mutex_t log_mutex; 

/* 获取当前进程名字 */
static int get_current_process_name(char *name, int len)
{
    char path[1024] = {0};
    char *real_name = NULL;

    if(readlink("/proc/self/exe", path, sizeof(path)) <=0)
        return -1;

    real_name = strrchr(path, '/');
    strncpy(name, real_name, len);
    return 0;
}

/* 获取配置文件名 */
static int get_cfg_name(char *name, int len)
{
    char process_name[128] = {0};
    int ret = 0;

    ret = get_current_process_name(process_name, sizeof(process_name));
    if(ret < 0)
        return -1;
    snprintf(name, len, "%s%s_log.conf", LOG_CONFIG_DIR, process_name);

    return 0;
}

static void log_lock(void *lock, int onoff) 
{
    if(0 == onoff) //off
        pthread_mutex_unlock(lock);
    else
        pthread_mutex_lock(lock);
}

static char *strip(char *s)
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

/* 从给定的字符串里解析出name=value的内容 */
static inline void get_name_value(char *line, char **name, char **value)
{
    char *p = NULL;

    *name = line;
    p = strchr(line, '=');
    if(!p)
    {
        *value = line;
        return;
    }
    *p = '\0';
    *value = p + 1;
    *value = strip(*value);
}

void parse_log_config(void)
{
    static int lock_inited = 0;  
    FILE *fp = NULL;   
    char cfg_name[128] = {0};
    char line[256] = {0};
    char *name = "NULL", *value = "NULL";

    /* 初始化线程锁 */
    if(0 == lock_inited)
    {
        pthread_mutex_init(&log_mutex, NULL);
        log_set_lock(log_lock);
        log_set_udata(&log_mutex);
        lock_inited = 1;
    }

    if(get_cfg_name(cfg_name, sizeof(cfg_name)))
        return;

    if(!(fp = fopen(cfg_name, "r")))
        return;

    while(fgets(line, sizeof(line), fp))
    {
        get_name_value(line, &name, &value);
        if(!strcmp(name, "level"))
        {
            log_set_level(atoi(value));
        }
        else if(!strcmp(name, "file"))
        {
            log_set_file(value);
        }
        else if(!strcmp(name, "size"))
        {
            log_set_file_size_limit(atoi(value));
        }
        memset(line, 0x0, sizeof(line));
    }

    fclose(fp);
}

static void cp(FILE *fp1, FILE *fp2)
{
    char buffer[8192] = {0};
    int len = 0;

    while(!feof(fp1))
    {
        len = fread(buffer, sizeof(char), sizeof(buffer), fp1);
        fwrite(buffer, sizeof(char), len, fp2);
    }
}

/* 文件过大，会截断之前的文件, max_size为字节 */
void file_size_limit_check(const char *path, int max_size)
{
    struct stat st = {0};
    FILE *log_fp = NULL, *tmp_fp = NULL;
    char tmp_name[128] = {0};
    int cut_len = 0;
    int write_len = 0;

    if(!path || max_size <= 0)
        return;

    if(-1 == stat(path, &st))
        return;

    if(st.st_size < max_size)
        return;

    log_fp = fopen(path, "r");
    if(!log_fp)
        return;

    /* 截断 */
    cut_len = st.st_size - max_size;
    snprintf(tmp_name, sizeof(tmp_name), "%s.new", path);
    tmp_fp = fopen(tmp_name, "w");
    if(!tmp_fp)
        goto error;
    fseek(log_fp, cut_len, SEEK_SET);
    cp(log_fp, tmp_fp);

    fclose(log_fp);
    fclose(tmp_fp);

    rename(tmp_name, path);
    return;

error:
    log_fp ? fclose(log_fp) : 0;
    tmp_fp ? fclose(tmp_fp) : 0;
    return;
}
