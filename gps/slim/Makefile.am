# Makefile.am - Automake script for slim_daemon

AM_CFLAGS = \
            -DFEATURE_COARSE_POS_TIMESTAMP_AGE \
            -DFEATURE_LOC_MW_EFS_ACCESS \
            -DFEATURE_RPC_CHAR_ARRAY \
            -DFEATURE_GNSS_BIT_API \
            -DFEATURE_LOCTECH_SLIM_NOHANDLES \
            -DFEATURE_LOCTECH_SLIM_LOG_CONSOLE \
            -DSLIM_API="" \
            -DLOG_TAG=\"slim\" \
            -D_GNU_SOURCE \
            -I./apss/daemon \
            -I./provider/common \
            $(LOCHAL_CFLAGS) \
            $(SLIMUTILS_CFLAGS) \
            $(SLIMCOMMON_CFLAGS) \
            $(SLIMCLIENT_CFLAGS) \
            $(QMIF_CFLAGS) \
            $(DIAG_CFLAGS) \
			-D__func__=__PRETTY_FUNCTION__ \
            -DHAVE_LIBLOG

AM_CPPFLAGS = \
            -D__STDC_FORMAT_MACROS \
            -D__STDINT_LIMITS \
                        -DFEATURE_LOCTECH_SLIM_LOG_CONSOLE \
            -D__STDINT_MACROS \
                        -DFEATURE_LOCTECH_QMISLIM_PROVIDER_TEST \
                        -DFEATURE_LOCTECH_SLIM_DEBUG

c_sources = \
            apss/daemon/SlimDaemonManager.cpp \
            apss/listener/ClientListener.cpp \
            provider/common/ProviderBase.cpp \
            provider/common/MultiplexingProvider.cpp \
            provider/common/SlimProviderConf.cpp \
            provider/common/KalmanFilter.cpp

# QMI-SLIM Provider Support
if FEATURE_LOCTECH_QMISLIM_PROVIDER_MP
AM_CFLAGS += \
            -DFEATURE_LOCTECH_QMISLIM_PROVIDER_MP \
            -I$(WORKSPACE)/gps/slim/provider/sensor1
endif

# Sensor1 Provider Support
if FEATURE_LOCTECH_SLIM_DSPS
AM_CFLAGS += \
            -DFEATURE_LOCTECH_SLIM_DSPS
c_sources += \
             provider/sensor1/SlimSensor1Provider.cpp
endif

# Vehicle Network Provider Support
if FEATURE_LOCTECH_SLIM_VNW
AM_CFLAGS += \
            -DFEATURE_LOCTECH_SLIM_VNW
c_sources += \
            provider/vnw/VehicleNetworkProvider.cpp \
            provider/vnw/VehicleNetworkConfiguration.cpp \
            provider/vnw/VehicleNetworkMessageProcessor.cpp \
            provider/vnw/BitStreamReader.cpp

endif

# QMI-LOC Modem Client Support
if FEATURE_LOCTECH_SLIM_QMILOC
AM_CFLAGS += \
            -DFEATURE_LOCTECH_SLIM_QMILOC
#h_sources += \
c_sources += \
            apss/listener/QLClientListener.cpp
endif

requiredlibs = -lrt $(LOCHAL_LIBS) $(SLIMUTILS_LIBS) $(SLIMCOMMON_LIBS) $(SLIMCLIENT_LIBS)


library_includedir = $(pkgincludedir)
library_include_HEADERS = $(h_sources)

slim_daemon_SOURCES = $(c_sources) $(h_sources)

if USE_GLIB
slim_daemon_CFLAGS = -DUSE_GLIB $(AM_CFLAGS) @GLIB_CFLAGS@
slim_daemon_LDFLAGS = -lstdc++ -lpthread @GLIB_LIBS@ -shared -version-info 1:0:0
slim_daemon_CPPFLAGS = -DUSE_GLIB $(AM_CFLAGS) $(AM_CPPFLAGS) @GLIB_CFLAGS@
else
slim_daemon_CFLAGS = $(AM_CFLAGS)
slim_daemon_LDFLAGS = -shared -version-info 1:0:0
slim_daemon_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS)
endif

slim_daemon_LDADD = $(requiredlibs)

bin_PROGRAMS = slim_daemon
