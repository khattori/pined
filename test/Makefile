TARGET	= pstest
SRCS	= pstest.c

CFLAGS	= -Wall -g
LIBS	= -lpinesock -lpthread

OBJS	= $(SRCS:.c=.o)

$(TARGET)	: $(OBJS)
	$(CC) -g -o $@ $(OBJS) $(LIBS)

clean	:
	rm -fr $(OBJS) $(TARGET)

clean-all	:
	rm -fr $(OBJS) $(TARGET) *~
