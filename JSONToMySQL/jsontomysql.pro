#-------------------------------------------------
#
# Project created by QtCreator 2014-04-23T16:17:53
#
#-------------------------------------------------

QT       += core xml sql

QT       -= gui

TARGET = jsontomysql
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

unix:INCLUDEPATH += /usr/include ../3rdparty
unix:LIBS += -L/usr/lib64 -l qjson

SOURCES += main.cpp
