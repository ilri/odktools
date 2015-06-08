#-------------------------------------------------
#
# Project created by QtCreator 2014-02-02T22:46:38
#
#-------------------------------------------------

QT       += core xml

QT       -= gui

TARGET = odktomysql
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

unix:INCLUDEPATH += ../3rdparty

include(../3rdparty/QtXlsxWriter/src/xlsx/qtxlsx.pri)

SOURCES += main.cpp
