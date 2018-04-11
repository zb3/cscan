export CC=gcc
export CFLAGS=-Wall -Wextra -Wno-unused-parameter -O2
export LDFLAGS=
export THREAD_LIB=-lpthread

export MODULES = print
ENABLED_MODULES = print
MODULES_CFG = 
MODULES_DIR = modules

OBJS=cscan.o hardcoded.o socket_list.o queue.o utils.o socket_utils.o rand.o target.o log.o cba.o
TARGETS=cscan

ifneq ($(MUSL),)
CC = musl-gcc
CFLAGS += -static
LDFLAGS += -static
TARGETS += strip
endif

ifneq ($(METRICS),)
OBJS += metrics.o
CFLAGS += -DMETRICS
endif

ifneq ($(DEBUG),)
CFLAGS += -ggdb
LDFLAGS += -ggdb
endif

ifneq ($(GPROF),)
CFLAGS += -pg -no-pie
LDFLAGS += -pg -no-pie
endif

all: $(TARGETS)

MODULE_LIB = $(MODULES_DIR)/modules.a

$(MODULE_LIB): $(OBJS) $(MODULES_DIR)
	$(MAKE) -C $(MODULES_DIR)

cscan: $(OBJS) $(MODULE_LIB)
	$(CC) $(LDFLAGS) $(OBJS) $(MODULE_LIB) $(THREAD_LIB) -o $@

target_gen_test: rand_for_test.o target.o target_gen_test.o utils.o
	$(CC) $(LDFLAGS) $^ -o $@
	
queue_test: queue.o queue_test.o
	$(CC) $(LDFLAGS) $(THREAD_LIB) $^ -o $@
	
socket_list_test: socket_list.o socket_list_test.o
	$(CC) $(LDFLAGS) $^ -o $@
	
cba_tester: cba.o cba_tester.o socket_utils.o utils.o
	$(CC) $(LDFLAGS) $^ -o $@


cscan.o: cscan.c
	$(CC) $(CFLAGS) -DENABLED_MODULES='"''$(ENABLED_MODULES)''"' -DMODULES_CFG='"''$(MODULES_CFG)''"' -c $< -o $@
	

socket_utils.o: socket_utils.c socket_utils.h
utils.o: utils.c utils.h
rand_for_test.o: rand_for_test.c rand.h
target.o: target.c target.h
target_gen_test.o: target_gen_test.c
queue_test.o: queue_test.c
queue.o: queue.c queue.h
socket_list.o: socket_list.c socket_list.h
metrics.o: metrics.c metrics.h
cba.o: cba.c cba.h

modules_test:
	$(MAKE) -C $(MODULES_DIR) test

TEST_PROGS = target_gen_test queue_test socket_list_test cba_tester modules_test
test: $(TEST_PROGS)
	
strip: cscan
	strip $<
	
clean:
	rm -f *.o
	rm -f cscan
	rm -f $(TEST_PROGS)
	make -C $(MODULES_DIR) clean
	
	
.PHONY: clean strip test $(MODULES_DIR) modules_test
	