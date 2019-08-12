/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "easy_log.h"

static struct {
  void *udata;
  log_LockFn lock;
  char file_name[128];
  int level;
  int quiet;
  int file_size_limit;
} L = {
  .level = LLOG_INFO,
};


static const char *level_names[] = {
  "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[35m", "\x1b[31m", "\x1b[33m", "\x1b[32m", "\x1b[36m", "\x1b[94m", 
};
#endif


static void lock(void)   {
  if (L.lock) {
    L.lock(L.udata, 1);
  }
}


static void unlock(void) {
  if (L.lock) {
    L.lock(L.udata, 0);
  }
}


void log_set_udata(void *udata) {
  L.udata = udata;
}


void log_set_lock(log_LockFn fn) {
  L.lock = fn;
}


void log_set_file(char *file_name) {
  strncpy(L.file_name, file_name, sizeof(L.file_name));
}

void log_set_file_size_limit(int limit)
{
  L.file_size_limit = limit;
}

void log_set_level(int level) {
  L.level = level;
}


void log_set_quiet(int enable) {
  L.quiet = enable ? 1 : 0;
}

void get_uptime_format(char *buf, int len)
{
  int s = 0, ms = 0;
  char line[128] = {0};
  FILE *fp = fopen("/proc/uptime", "r");

  if(!fp)
  {
    snprintf(buf, len, "Unknow");
    return;
  }

  fgets(line, sizeof(line), fp);
  fclose(fp);

  s = atoi(line);
  ms = atoi(strchr(line, '.') + 1) * 10;
  snprintf(buf, len, "%02d:%02d:%02d.%03d", s/3600, s%3600/60, s%3600%60, ms);

  return;
}

void get_time_string(char *short_str, char *long_str, int short_len, int long_len)
{
  /* 如果当前系统时间正常，那么返回系统时间，否则返回uptime时间 */
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);

  if(t < 1532684064)  /* 2018/7/27 17:34:19 */
  {
    get_uptime_format(short_str, short_len);
    get_uptime_format(long_str, long_len);
  }
  else
  {
    strftime(long_str, long_len, "%y-%m-%d %H:%M:%S", lt);
    strftime(short_str, short_len, "%H:%M:%S", lt);
  }
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {

  /* 解析配置文件，实现命令行可以动态的调整等级 */
  parse_log_config();
  if (level > L.level) {
    return;
  }

  /* Acquire lock */
  lock();

  char short_time[32] = {0};
  char long_time[32] = {0};
  get_time_string(short_time, long_time, sizeof(short_time), sizeof(long_time));
  /* Log to stderr */
  if (!L.quiet) {
    va_list args;
    
#ifdef LOG_USE_COLOR
    fprintf(
      stdout, "%s %s%-7s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
      short_time, level_colors[level], level_names[level], file, line);
#else
    fprintf(stdout, "%s %-7s %s:%d: ", short_time, level_names[level], file, line);
#endif
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    //fprintf(stdout, "\n");
    fflush(stdout);
  }

  /* Log to file */
  if (L.file_name[0]) {
    va_list args;
    FILE *fp = fopen(L.file_name, "a+");
    if(!fp)
      goto out;

    fprintf(fp, "%17s %-7s %s:%d: ", long_time, level_names[level], file, line);
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    //fprintf(fp, "\n");
    fflush(fp);
    fclose(fp);

    /* 大小检查 */
    file_size_limit_check(L.file_name, L.file_size_limit * 1024);
  }

out:
  /* Release lock */
  unlock();
}
