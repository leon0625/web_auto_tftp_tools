#
# 编译库和测试文件
# 

export CC ?= gcc
export TOP_DIR=$(shell pwd)
export CFLAGS += -MMD -fPIC -I$(TOP_DIR)/uloop  -I$(TOP_DIR)/log
export LDFLAGS += -L$(TOP_DIR)/uloop -luloop -L$(TOP_DIR)/log -leasy_log 

apps_module_dir_y = uloop log client server


all: $(apps_module_dir_y)
	for i in $(apps_module_dir_y) ; \
	do make -C $$i $@ || exit $?; \
	done
	@echo "OK!"

client_only: uloop log client
	for i in $^ ; \
	do make -C $$i all || exit $?; \
	done
	@echo "OK!"

server_only: uloop log server
	for i in $^ ; \
	do make -C $$i all || exit $?; \
	done
	@echo "OK!"

clean: 
	for i in $(apps_module_dir_y) ; \
	do make -C $$i $@ || exit $?; \
	done


.PHONY: $(apps_module_dir_y)