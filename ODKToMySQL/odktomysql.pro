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

include(../3rdparty/QtXlsxWriter/src/xlsx/qtxlsx.pri)
INCLUDEPATH += ../3rdparty
LIBS += -lquazip5 -lcsv

SOURCES += main.cpp
