
CFLAGS:=-Wall
LDFLAGS:=-pthread
PROGS:=\
	server\
	client
ZIP:=hw5.zip

all: $(PROGS)

clean:
	$(RM) *.o $(PROGS) $(ZIP)

$(ZIP): clean
	cd .. && \
	zip -r $@ hw5/*.[ch] hw5/Makefile && \
	mv $@ hw5/.

.PHONY: all clean
