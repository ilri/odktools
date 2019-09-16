TEMPLATE = subdirs

CONFIG += debug

SUBDIRS = createFromXML \
    insertFromXML \
    MySQLDenormalize \
    MySQLToXLSX \
    createAuditTriggers \
    createDummyJSON \
    MySQLToSQLite \
    mergeVersions
