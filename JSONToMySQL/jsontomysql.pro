#-------------------------------------------------
#
# Project created by QtCreator 2014-04-23T16:17:53
#
#-------------------------------------------------

QT       += core xml sql qml

QT       -= gui

TARGET = jsontomysql
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

unix:INCLUDEPATH += /usr/include ../3rdparty
unix:LIBS += -L/usr/lib64 -l qjson-qt5

SOURCES += main.cpp \
    insertvalues.cpp \
    mainclass.cpp

HEADERS += \
    insertvalues.h \
    mainclass.h
