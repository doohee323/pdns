AM_CPPFLAGS += $(CDB_CFLAGS)

pkglib_LTLIBRARIES = libzmqbackend.la

EXTRA_DIST = \
	OBJECTFILES \
	OBJECTLIBS \
	backend.pl

libzmqbackend_la_SOURCES = \
	zmq.hpp \
	zhelpers.hpp \
	zexception.hh \
	zmqbackend.cc zmqbackend.hh

libzmqbackend_la_LDFLAGS = -module -avoid-version -lzmq
libzmqbackend_la_LIBADD = $(CDB_LIBS)


