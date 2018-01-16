#-------------------------------------------------
#
# Project created by QtCreator 2014-02-02T22:46:38
#
#-------------------------------------------------

QT       += core xml sql

QT       -= gui

TARGET = odktomysql
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

unix:INCLUDEPATH += ../3rdparty /usr/local/include /usr/include
unix:LIBS += -L/lib64 -L/usr/lib64 -L/usr/local/lib -lquazip5 -lcsv


include(../3rdparty/QtXlsxWriter/src/xlsx/qtxlsx.pri)

SOURCES += main.cpp
