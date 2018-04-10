AM_CFLAGS = \
    -DDEBUG \
    -I$(WORKSPACE)/gps/framework/native/service \
    $(QMIF_CFLAGS) \
    $(QMI_CFLAGS) \
    $(EVENTOBSERVER_CFLAGS) \
    $(LOCPLA_CFLAGS) \
    $(LOC_CFLAGS) \
    $(LOCVZW_CFLAGS) \
    $(GTI_CFLAGS) \
    $(LOCATIONSERVICE_CFLAGS) \
    $(ULP2_CFLAGS)\
    $(LOCGLUE_CFLAGS) \
    -DTEST_ULP

required_libs = \
    $(LOCPLA_LIBS) \
    $(LOC_LIBS) \
    $(LOCVZW_LIBS) \
    $(LOCATIONSERVICE_LIBS) \
    $(ULP2_LIBS) \
    $(LOCGLUE_LIBS)

garden_h_sources = \
     test_android_gps.h

garden_c_sources = \
     test_android_gps.cpp \
     fake_vzw_jni_bridage.cpp

#EXTRA_HEADERS = location_test_interface_base.h
#initial-header:
#    $(mkinstalldirs) $(DESTDIR)$(includedir)
#    $(INSTALL_DATA) $(EXTRA_HEADERS) $(DESTDIR)$(includedir)

garden_app_SOURCES = $(garden_c_sources) $(garden_h_sources)

if USE_GLIB
garden_app_CFLAGS = -DUSE_GLIB $(AM_CFLAGS) @GLIB_CFLAGS@
garden_app_LDFLAGS = -lstdc++ -g -lpthread @GLIB_LIBS@ -shared -version-info 1:0:0
garden_app_CPPFLAGS = -DUSE_GLIB $(AM_CFLAGS) $(AM_CPPFLAGS) @GLIB_CFLAGS@
else
garden_app_CFLAGS = $(AM_CFLAGS)
garden_app_LDFLAGS = -lpthread -shared -version-info 1:0:0
garden_app_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS)
endif

garden_app_LDADD = -lcutils -ldl $(LOC_LIBS) $(required_libs)

bin_PROGRAMS = garden_app
