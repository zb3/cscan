CFLAGS ?= -c
CFLAGS += -I../
CFLAGS += $(patsubst %,-DHAVE_%,$(MODULES))

LIB_OBJS =
WITH_TLS = 
TEST_PROGS = 

all: modules.a


ifneq ($(filter print,$(MODULES)),)
PRINT_OBJS = print.o
print.o: print.c print.h
LIB_OBJS += $(PRINT_OBJS)
endif


ifneq ($(filter checkcookie,$(MODULES)),)
COOKIE_OBJS = cookie.o
cookie.o: cookie.c cookie.h
cookie_tester: cookie_tester.o ../utils.o ../socket_utils.o modules.a
TEST_PROGS += cookie_tester
LIB_OBJS += $(COOKIE_OBJS)
endif 


ifneq ($(WITH_TLS),)
CFLAGS += -Imbedtls/include -DHAVE_TLS
endif 

LIB_OBJS += ts.o
ts.o: ts.c ts.h
ts_tester: ts_tester.o ../utils.o ../socket_utils.o modules.a
TEST_PROGS += ts_tester

comma := ,
space :=
space +=

modules.o: modules.c ../modules.h
	$(CC) $(CFLAGS) -DMODULES_STR='"''$(subst $(space),$(comma)$(space),$(MODULES))''"' -c $< -o $@

ifeq ($(WITH_TLS),)

modules.a: $(LIB_OBJS) modules.o
	rm -f $@
	ar rcs $@ $^

else

modules.a: $(LIB_OBJS) modules.o mbedtls/library/libmbedcrypto.a mbedtls/library/libmbedtls.a mbedtls/library/libmbedx509.a
	rm -f $@
	ar rcs $@ $^
	rm -rf libmerge
	mkdir libmerge
	cd libmerge && \
	ar x ../mbedtls/library/libmbedcrypto.a && \
	ar x ../mbedtls/library/libmbedtls.a && \
	ar x ../mbedtls/library/libmbedx509.a && \
	(for f in * ; do mv "$$f" "mbed_$$f" ; done) && \
	ar x ../modules.a && \
	ar rcs modules.a *o
	mv libmerge/$@ $@
	rm -rf libmerge
  

endif


test: $(TEST_PROGS)

clean:
	rm -f *.o *.a
	rm -f *_tester
	
.PHONY: all clean test