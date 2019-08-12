# 编译测试文件

EXEC = server
SRC = server.c
OBJS = $(SRC:%.c=%.o)

all: $(EXEC)
	@echo compiled!

$(EXEC): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -MMD $(CFLAGS) -c $< -o $@

clean:
	-rm -rf *.d *.o $(EXEC)

-include $(SRC:%.c=%.d)