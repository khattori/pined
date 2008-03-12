PREFIX	= /usr/local

TARGET	= pined
SRCS	= \
	pined.c	\
	session.c	\
	logger.c	\
	version.c

CFLAGS	= -Wall -g -I../
LIBS	= -lpthread

OBJS	= $(SRCS:.c=.o)

$(TARGET)	: $(OBJS)
	$(CC) -g -o $@ $(OBJS) $(LIBS)

install	:
	cp $(TARGET) $(PREFIX)/bin/$(TARGET)

.PHONY:	clean clean-all
clean	:
	rm -fr $(OBJS) $(TARGET)

clean-all	:
	rm -fr $(OBJS) $(TARGET) *~
