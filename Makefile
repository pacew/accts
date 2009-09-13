CFLAGS = -g -Wall -I/usr/include/postgresql
LIBS = -lpq -lofx -lopus -lm

DBNAME=wtrans

WDIR=/var/www/html/dev/wtrans
WWW_FILES = wtrans.php common.php style.css index.php splits.php trans.php \
	tags.php tag.php


all: import-ofx import-qif import-wasabe

import-ofx: import-ofx.o pglib.o
	$(CC) $(CFLAGS) -o import-ofx import-ofx.o pglib.o $(LIBS)

import-qif: import-qif.o pglib.o
	$(CC) $(CFLAGS) -o import-qif import-qif.o pglib.o $(LIBS)

import-wasabe: import-wasabe.o pglib.o
	$(CC) $(CFLAGS) -o import-wasabe import-wasabe.o pglib.o $(LIBS)


links:
	mkdir -p $(WDIR)
	for f in $(WWW_FILES); do ln -sf `pwd`/$$f $(WDIR)/.; done

dump:
	pg_dump --data-only $(DBNAME) > $(DBNAME).dump

restore:
	dropdb $(DBNAME)
	createdb -O apache $(DBNAME)
	psql -U apache -q -f schema $(DBNAME)
	psql -U apache -q -f $(DBNAME).dump $(DBNAME)

clean:
	rm -f *.o
