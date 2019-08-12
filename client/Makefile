# 编译测试文件

EXEC = client
SRC = client.c
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