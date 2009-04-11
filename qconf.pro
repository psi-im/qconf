QT      -= gui
QT      += xml
CONFIG  += console
CONFIG  -= app_bundle
DESTDIR  = $$PWD

HEADERS += src/stringhelp.h
SOURCES += src/stringhelp.cpp src/qconf.cpp

include($$OUT_PWD/conf.pri)

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
unix:!isEmpty(BINDIR) {
	#CONFIG += no_fixpath
	target.path = $$BINDIR
	INSTALLS += target
	libfiles.path = $$DATADIR/qconf
	libfiles.files = $$IN_PWD/conf $$IN_PWD/modules
	INSTALLS += libfiles
}
