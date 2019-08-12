#
# 编译库和测试文件
# 

LIB_ULOOP = libuloop.a
SRC = uloop.c utils.c usock.c

all: $(LIB_ULOOP)
	@echo compiled!

$(LIB_ULOOP): $(SRC:%.c=%.o)
	ar -rc $@ $^

clean: 
	-rm -rf *.d *.o *.a *.so $(LIB_ULOOP)

%.o: %.c
	$(CC) -MMD $(CFLAGS) -c $< -o $@

-include $(SRC:%.c=%.d)
