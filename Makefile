all: rigolif

rigolif: rigolif-posix.c
	gcc $? -ggdb -o $@ -Wall

clean:
	rm rigolif

