CONFIG += console
CONFIG -= qt app_bundle
TARGET = configexe_stub

CONFIG += release

*win32-g++*:QMAKE_LFLAGS += -static-libgcc

HEADERS += \
	embed.h

SOURCES += \
	embed.c \
	configexe.c
