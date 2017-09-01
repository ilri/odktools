#-------------------------------------------------
#
# Project created by QtCreator 2014-04-08T14:25:10
#
#-------------------------------------------------

QT       += core xml sql qml

QT       -= gui

TARGET = odkdatatomysql
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp insertvalues.cpp

INCLUDEPATH += ../3rdparty

HEADERS += \
    insertvalues.h
