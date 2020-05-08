
CFLAGS:=-Wall
LDFLAGS:=-pthread
PROGS:=\
	server\
	client
DIR:=$(notdir $(basename $(CURDIR)))
TAR:=$(DIR).tar.gz

SRC:=$(wildcard *.c)
DEP:=$(patsubst %.c,%.d,$(SRC))

all: $(PROGS)

server: server.o util.o

client: client.o util.o

-include $(DEP)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@


$(TAR): clean
	cd .. && \
	tar czf $@ $(DIR) && \
	mv $@ $(DIR)/.

clean:
	$(RM) *.o *.d $(PROGS) $(TAR)

.PHONY: all clean
