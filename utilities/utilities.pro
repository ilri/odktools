TEMPLATE = subdirs

CONFIG += debug

SUBDIRS = createFromXML \
    insertFromXML \
    MySQLDenormalize \
    MySQLToXLSX \
    MySQLToCSV \
    MySQLToSTATA \
    createAuditTriggers \
    createDummyJSON \
    MySQLToSQLite \
    mergeVersions \
    createTemporaryTable
