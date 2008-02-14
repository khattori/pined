TARGET	= echod
SRCS	= echod.c

CFLAGS	= -Wall -g 
LIBS	= -lpthread -lpinesock

OBJS	= $(SRCS:.c=.o)

$(TARGET)	: $(OBJS)
	$(CC) -g -o $@ $(OBJS) $(LIBS)

clean	:
	rm -fr $(OBJS) $(TARGET)
