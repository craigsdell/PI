# a little hacky: the headers are in the parent of the configure subdir
AM_CPPFLAGS += \
-I$(top_srcdir)/../../include -I$(NP4_INSTALL)/include/np4

libpi_np4_la_SOURCES = \
pi_imp.cpp \
pi_act_prof_imp.cpp \
pi_counter_imp.cpp \
pi_learn_imp.cpp \
pi_mc_imp.cpp \
pi_meter_imp.cpp \
pi_tables_imp.cpp \
pi_clone_imp.cpp \
device_mgr.cpp \
device.cpp \
tables.cpp \
np4_intel_device_config.pb.cc

libpi_np4_la_LIBADD = \
$(top_builddir)/../../src/libpip4info.la \
$(NP4_INSTALL)/lib/libp4dev.la


lib_LTLIBRARIES = libpi_np4.la

distclean-local: distclean-ax-prefix-config-h
distclean-ax-prefix-config-h:
	rm -f pi-np4-config.h
