TEMPLATE = subdirs

CONFIG += debug

SUBDIRS = createFromXML \
    insertFromXML \
    MySQLDenormalize \
    MySQLToXLSX \
    MySQLToCSV \
    MySQLToJSON \
    MySQLToSTATA \
    createAuditTriggers \
    createDummyJSON \
    MySQLToSQLite \
    mergeVersions \
    createTemporaryTable
