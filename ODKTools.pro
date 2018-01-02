TEMPLATE = subdirs

CONFIG += debug

SUBDIRS = ODKToMySQL/odktomysql.pro \
JSONToMySQL/jsontomysql.pro \
    utilities \
    XMLtoJSON/xmltojson.pro \
    MySQLDenormalize/MySQLDenormalize.pro
