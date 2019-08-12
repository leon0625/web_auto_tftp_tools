/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#define LOG_USE_COLOR 1
#define LOG_VERSION "0.1.0"

#define LOG_CONFIG_DIR "/var/log_conf/"

typedef void (*log_LockFn)(void *udata, int lock);

enum { 
    LLOG_FATAL = 0, 
    LLOG_ERROR, 
    LLOG_WARN, 
    LLOG_INFO, 
    LLOG_DEBUG, 
    LLOG_VERBOSE, 
};

/*
    使用方法：
    在代码里面直接使用, log_verbose(), log_debug() ...这些函数即可，默认输出到
    标准错误输出，日志等级LLOG_INFO。

    动态配置日志等级
    在LOG_CONFIG_DIR宏的目录下添加配置文件，配置文件名为"进程名_log.conf"
    支持三个参数配置，level，日志等级，如果有file，表示输出到文件，size代表显示文件大小

    例子：
    配置文件名：/var/log_conf/a.out_log.conf
    level=3     #日志等级3
    file=/var/123.log   #日志是否保存到文件，不保存到文件没有这行
    size=100k       # 日志文件大小限制，单位KB，可以没有
 */

#define log_verbose(...) log_log(LLOG_VERBOSE, __func__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LLOG_DEBUG, __func__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LLOG_INFO,  __func__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LLOG_WARN,  __func__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LLOG_ERROR, __func__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LLOG_FATAL, __func__, __LINE__, __VA_ARGS__)

void log_set_udata(void *udata);
void log_set_lock(log_LockFn fn);
void log_set_file(char *file_name);
void log_set_file_size_limit(int limit);    //单位KB
void log_set_level(int level);
void log_set_quiet(int enable);
void parse_log_config(void);
void file_size_limit_check(const char *path, int max_size);

void log_log(int level, const char *file, int line, const char *fmt, ...);

#endif
