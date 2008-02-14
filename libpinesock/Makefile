INCLUDES= -I..
LDFLAGS = -g -shared

PREFIX	= /usr/local

CFLAGS	= -Wall -g $(INCLUDES) -fPIC

TARGET	= libpinesock.so

SRCS	= pinesock.c
INCS	= pinesock.h

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
