
CFLAGS:=-Wall
LDFLAGS:=-pthread
PROGS:=\
	server\
	client
DIR:=$(notdir $(basename $(CURDIR)))
TAR:=$(DIR).tar.gz

all: $(PROGS)

clean:
	$(RM) *.o $(PROGS) $(TAR)

$(TAR): clean
	cd .. && \
	tar czf $@ $(DIR) && \
	mv $@ $(DIR)/.

dist: $(TAR)

.PHONY: all clean dist
