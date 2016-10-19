QT      -= gui
QT      += xml
CONFIG  += console
CONFIG  -= app_bundle
DESTDIR  = $$PWD

HEADERS += src/stringhelp.h
SOURCES += src/stringhelp.cpp src/qconf.cpp

exists($$OUT_PWD/conf.pri) {
	include($$OUT_PWD/conf.pri)
}

isEmpty(CXXFLAGS) {
	CXXFLAGS=$$(CXXFLAGS)
}

!isEmpty(CXXFLAGS) {
	QMAKE_CXXFLAGS = $$CXXFLAGS
	QMAKE_CXXFLAGS_DEBUG = $$CXXFLAGS
	QMAKE_CXXFLAGS_RELEASE = $$CXXFLAGS
}

isEmpty(LDFLAGS) {
	LDFLAGS=$$(LDFLAGS)
}

!isEmpty(LDFLAGS) {
	QMAKE_LFLAGS = $$LDFLAGS
}

!isEmpty(DATADIR) {
	contains($$list($$[QT_VERSION]), 4.0.*|4.1.*) {
		DEFINES += DATADIR=\"$$DATADIR\"
	} else {
		DEFINES += DATADIR=\\\"$$DATADIR\\\"
	}
}

RESOURCES += src/qconf.qrc

# install
# we check for empty BINDIR here in case we're debugging with configexe on unix
!isEmpty(BINDIR):!isEmpty(DATADIR) {
	#CONFIG += no_fixpath
	target.path = $$BINDIR
	INSTALLS += target
	libfiles.path = $$DATADIR/qconf
	libfiles.files = $$IN_PWD/conf $$IN_PWD/modules
	INSTALLS += libfiles
}
