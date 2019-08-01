TEMPLATE = subdirs

CONFIG += debug

SUBDIRS = compareCreateXML \
    compareInsertXML \
    createFromXML \
    insertFromXML \
    MySQLDenormalize \
    JSONToCSV \
    MySQLToXLSX \ 
    createDummyJSON \
    createAuditTriggers \
    MySQLToSQLite
