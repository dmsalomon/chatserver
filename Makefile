
CFLAGS:=-Wall
LDFLAGS:=-pthread
PROGS:=\
	server\
	client
DIR:=$(notdir $(basename $(CURDIR)))
ZIP:=$(DIR).zip

all: $(PROGS)
	echo $(DIR)

clean:
	$(RM) *.o $(PROGS) $(ZIP)

$(ZIP): clean
	cd .. && \
	zip -r $@ $(DIR)/*.[ch] $(DIR)/Makefile && \
	mv $@ $(DIR)/.

.PHONY: all clean
