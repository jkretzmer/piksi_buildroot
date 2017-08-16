TARGET=sbp_json_lws
SOURCES= \
	sbp_json_lws-list.c \

LIBS=-lczmq -lsbp -lpiksi -lwebsockets -lpthread
CFLAGS=-std=gnu11

CROSS=

CC=$(CROSS)gcc

all:	$(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -rf $(TARGET)
