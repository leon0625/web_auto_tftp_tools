#
# 编译库和测试文件
# 

LIB_EASYLOG = libeasy_log.a
SRC = easy_log.c easy_log_extend.c

all: $(LIB_EASYLOG)
	@echo compiled!

$(LIB_EASYLOG): $(SRC:%.c=%.o)
	ar -rc $@ $^

clean: 
	-rm -rf *.d *.o *.a *.so $(LIB_EASYLOG)

%.o: %.c
	$(CC) -MMD $(CFLAGS) -c $< -o $@

-include $(SRC:%.c=%.d)
