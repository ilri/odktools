TEMPLATE = subdirs

CONFIG += debug

SUBDIRS = createFromXML \
    insertFromXML \
    MySQLDenormalize \
    JSONToCSV \
    MySQLToXLSX \
    createAuditTriggers \
    createDummyJSON \
    MySQLToSQLite \
    mergeVersions
