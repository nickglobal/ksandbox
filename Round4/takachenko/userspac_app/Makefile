CC := arm-linux-gnueabi-gcc 

APP = gyro2disp

INCDIR := include
SRCDIR := src
CFLAGS := -Wall -static
LDFLAGS:= -lpthread
LIBS   :=
OBJS   := $(patsubst ../../%, %, $(wildcard $(addsuffix /*.c, $(SRCDIR))))
OBJS   := $(OBJS:.c=.o)
 
all: $(APP)
	 
$(APP): $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS) $(LIBS)
			 
%.o : %.c
	$(CC) -o $@ -c $< $(CFLAGS) $(addprefix -I, $(INCDIR))
			 
.PHONY : clean
	 
clean:
	@rm -rf $(APP) $(SRCDIR)/*.o
