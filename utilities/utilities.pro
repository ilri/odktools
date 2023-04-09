TEMPLATE = subdirs

CONFIG += debug

SUBDIRS = createFromXML \
    DCFToODK \
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
    createTemporaryTable \
    DCFToODK
