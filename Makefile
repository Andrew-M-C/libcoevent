
# gcc compiler
MAKE = make
CC  = gcc
CPP = g++
LD  = ld

# target
TARGET_SO = bin/libcoevent.so
TARGET_A = bin/libcoevent.a

# directories
BIN_DIR = ./bin
LIBCO_DIR = ./libco_from_git
LIBCO_TARGET = $(LIBCO_DIR)/lib/libcolib.a
LIBCO_GIT_URL = https://github.com/Tencent/libco.git

# flags
CFLAGS += -Wall -g -I../cheaders -I$(LICENSE_DIR)/src -fPIC -lpthread $(OPENSSL_CFLAGS)
CPPFLAGS += -Wall -fpermissive -g -I../cheaders -I$(LICENSE_DIR)/src -fPIC -lpthread $(OPENSSL_CFLAGS)
LDFLAGS += -lpthread -lm -lrt

# source files
C_SRCS = $(wildcard *.c)
CPP_SRCS = $(wildcard *.cpp) $(LICENSE_DIR)/src/hashmd5.cpp $(LICENSE_DIR)/src/SSLKernelItem.cpp
ASM_SRCS = $(wildcard *.S)

C_OBJS = $(C_SRCS:.c=.o)
CPP_OBJS = $(CPP_SRCS:.cpp=.o)
ASM_OBJS = $(ASM_SRCS:.S=.o)

NULL ?=#
ifneq ($(strip $(CPP_OBJS)), $(NULL))
FINAL_CC = $(CPP)
else
FINAL_CC = $(CC)
CPPFLAGS = $(CFLAGS)
endif

export FINAL_CC
export NULL
export CPPFLAGS
export CFLAGS
export CC
export CPP
export LD

# default target
.PHONY:all
all: $(BIN_DIR) $(TARGET_SO) $(TARGET_A)
	@echo "	<< $(shell basename `pwd`) made >>"

# libco
$(LIBCO_DIR):
	git clone 'https://github.com/Tencent/libco.git' $(LIBCO_DIR)

$(LIBCO_TARGET): $(LIBCO_DIR)
	if [ ! -f $(LIBCO_TARGET) ]; then\
		make -C $(LIBCO_DIR);\
	fi

# automatic compiler
-include $(C_OBJS:.o=.d)
-include $(CPP_OBJS:.o=.d)

$(CPP_OBJS): $(CPP_OBJS:.o=.cpp)
	$(CPP) -c $(CPPFLAGS) $*.cpp -o $*.o  
	@$(CPP) -MM $(CPPFLAGS) $*.cpp > $*.d  
	@mv -f $*.d $*.d.tmp  
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d  
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp 

$(C_OBJS): $(C_OBJS:.o=.c)
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	@$(CC) -MM $(CFLAGS) $*.c > $*.d  
	@mv -f $*.d $*.d.tmp  
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d  
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp 

$(ASM_OBJS): $(ASM_OBJS:.o=.S)
	$(CC) -c $*.S

#$(PROG_NAME): $(C_OBJS) $(CPP_OBJS) $(ASM_OBJS)
#	@echo "$(LD) -r -o $@.o *.o"
#	@$(LD) -r -o $@.o $(C_OBJS) $(CPP_OBJS) $(ASM_OBJS)
#	$(FINAL_CC) $@.o $(STATIC_LIBS) -o $@ $(LDFLAGS)
#	chmod $(TARMODE) $@

.PHONY: clean
clean:
#	@rm -f $(C_OBJS) $(CPP_OBJS) $(PROG_NAME) clist.txt cpplist.txt *.d *.d.* *.o
	-make clean -C $(LIBCO_DIR)
	-@find -name '*.o' | xargs -I [] rm [] >> /dev/null
	-@find -name '*.d' | xargs -I [] rm [] >> /dev/null
#	-@find -name '*.so' | xargs -I [] rm [] >> /dev/null
	-@rm -f $(TARGET_SO) $(TARGET_A)
	@echo "	<< $(shell basename `pwd`) cleaned >>"

.PHONY: distclean
distclean: clean
	rm -rf $(LIBCO_DIR)



.PHONY: test
test:
	@echo 'test'

