PREFIX	= /usr/local

TARGET	= rserver
SRCS	= \
	rserver.c	\
	session.c	\
	logger.c

CFLAGS	= -Wall -g -I../
LIBS	= -lpthread

OBJS	= $(SRCS:.c=.o)

$(TARGET)	: $(OBJS)
	$(CC) -g -o $@ $(OBJS) $(LIBS)

install	:
	cp $(TARGET) $(PREFIX)/bin/$(TARGET)

clean	:
	rm -fr $(OBJS) $(TARGET)
