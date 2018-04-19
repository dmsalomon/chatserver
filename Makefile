
CFLAGS:=-Wall
LDFLAGS:=-pthread
PROGS:=\
	server\
	client
DIR:=$(notdir $(basename $(CURDIR)))
ZIP:=$(DIR).zip

all: $(PROGS)

clean:
	$(RM) *.o $(PROGS) $(ZIP)

$(ZIP): clean
	cd .. && \
	zip -r $@ $(DIR)/*.[ch] $(DIR)/Makefile && \
	mv $@ $(DIR)/.

dist: $(ZIP)

.PHONY: all clean dist
