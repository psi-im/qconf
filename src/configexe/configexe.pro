CONFIG += console
CONFIG -= qt app_bundle
TARGET = configexe_stub

CONFIG += release

HEADERS += \
	embed.h

SOURCES += \
	embed.c \
	configexe.c
