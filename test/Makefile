TARGET	= rstest
SRCS	= rstest.c

CFLAGS	= -Wall -g -I../rssock/ -L../rssock/
LIBS	= -lrssock

OBJS	= $(SRCS:.c=.o)

$(TARGET)	: $(OBJS)
	$(CC) -g -o $@ $(OBJS) $(LIBS)

clean	:
	rm -fr $(OBJS) $(TARGET)
