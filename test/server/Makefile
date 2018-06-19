
# gcc compiler
MAKE = make
CC  = gcc
CPP = g++
LD  = ld

# target
TARGET_BIN = server

# flagsst 
CFLAGS += -Wall -g -fPIC -lpthread -I../../include -I./ -I../../libco_from_git
CPPFLAGS += $(CFLAGS)
LDFLAGS += -Wl,-Bstatic -lcoevent -L../../bin/ -lcolib -L../../libco_from_git/lib -Wl,-Bdynamic -lpthread -lm -lrt -levent -ldl

# source files
C_SRCS = $(wildcard ./*.c)
CPP_SRCS = $(wildcard ./*.cpp)
ASM_SRCS = $(wildcard ./*.S)

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
all: $(TARGET_BIN)
	@echo "	<< $(TARGET_BIN) made >>"

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

../../bin/libcoevent.a:
	make -C ../../

# server
$(TARGET_BIN): $(C_OBJS) $(CPP_OBJS) ../../bin/libcoevent.a
	@echo "$(LD) -r -o $@.o *.o"
	@$(LD) -r -o $@.o $(C_OBJS) $(CPP_OBJS)
	$(FINAL_CC) $@.o $(STATIC_LIBS) -o $@ $(LDFLAGS)
	chmod +x $@

.PHONY: clean
clean:
#	@rm -f $(C_OBJS) $(CPP_OBJS) $(PROG_NAME) clist.txt cpplist.txt *.d *.d.* *.o
	-@find -name '*.o' | xargs -I [] rm [] >> /dev/null
	-@find -name '*.d' | xargs -I [] rm [] >> /dev/null
#	-@find -name '*.so' | xargs -I [] rm [] >> /dev/null
	-@rm -f $(TARGET_BIN)
	@echo "	<< $(TARGET_BIN) cleaned >>"

.PHONY: distclean
distclean: clean
	rm -rf $(LIBCO_DIR)

.PHONY: test
test:
	@echo 'test'
	@echo $(CPP_OBJS) $(C_OBJS)

