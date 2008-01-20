INCLUDES= -I..
LDFLAGS = -g -shared

PREFIX	= /usr/local

CFLAGS	= -Wall -g $(INCLUDES) -fPIC

TARGET	= librssock.so

SRCS	= rssock.c
INCS	= rssock.h

OBJS	= $(SRCS:.c=.o)

$(TARGET)	: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install	: $(TARGET)
	@echo "copying library and header file..."
	@cp $(TARGET) $(PREFIX)/lib/
	@cp $(INCS) $(PREFIX)/include/
	@/sbin/ldconfig
	@echo "done."

clean	:
	rm -fr $(OBJS) $(TARGET)
