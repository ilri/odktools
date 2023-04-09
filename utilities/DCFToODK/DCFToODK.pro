QT       += core xml

QT       -= gui

TARGET = dcftoodk
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += ../../3rdparty

SOURCES += main.cpp \
    dcftoxml.cpp \
    xmltoyml.cpp

HEADERS += \
    dcftoxml.h \
    xmltoyml.h
