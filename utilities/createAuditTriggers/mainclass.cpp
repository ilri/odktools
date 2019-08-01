#include "mainclass.h"
#include <QDir>
#include <QSqlQuery>
#include <QTextStream>
#include <QUuid>

mainClass::mainClass(QObject *parent) : QObject(parent)
{
    returnCode = 0;
}

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString outputDirectory)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
    this->schema = schema;
    this->outputDirectory = outputDirectory;
}

void mainClass::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s", temp.toUtf8().data());
}

int mainClass::createAudit(QSqlDatabase mydb, QString auditDir, QStringList ignoreTables)
{
    QSqlQuery query(mydb);
    QSqlQuery query2(mydb);
    QString sql;
    QStringList TriggerData;
    QStringList TriggerLite;

    QStringList dropMyTriggers;
    QStringList dropLiteTriggers;

    TriggerData << "CREATE TABLE IF NOT EXISTS audit_log (";
    TriggerData << "audit_id VARCHAR(64) ,";
    TriggerData << "audit_date TIMESTAMP(6) NULL ,";
    TriggerData << "audit_action VARCHAR(6) NULL ,";
    TriggerData << "audit_user VARCHAR(120) NULL ,";
    TriggerData << "audit_table VARCHAR(120) NULL ,";
    TriggerData << "audit_column VARCHAR(120) NULL ,";
    TriggerData << "audit_key VARCHAR(64) NULL ,";
    TriggerData << "audit_oldvalue TEXT NULL ,";
    TriggerData << "audit_newvalue TEXT NULL ,";
    TriggerData << "audit_insdeldata TEXT NULL ,";
    TriggerData << "PRIMARY KEY (audit_id) )";
    TriggerData << " ENGINE = InnoDB CHARSET=utf8;";
    TriggerData << "";

    //TriggerData << "ALTER TABLE audit_log ADD COLUMN audit_insdeldata TEXT NULL;";
    //TriggerData << "ALTER TABLE audit_log CHANGE audit_updatekey audit_key varchar(500);";
    //TriggerData << "";


    TriggerLite << "BEGIN;";
    TriggerLite << "CREATE TABLE audit_log (";
    TriggerLite << "audit_id VARCHAR(64) NOT NULL,";
    TriggerLite << "audit_date VARCHAR(120) NULL ,";
    TriggerLite << "audit_action VARCHAR(6) NULL ,";
    TriggerLite << "audit_user VARCHAR(120) NULL ,";
    TriggerLite << "audit_table VARCHAR(120) NULL ,";
    TriggerLite << "audit_column VARCHAR(120) NULL ,";
    TriggerLite << "audit_key VARCHAR(64) NULL ,";
    TriggerLite << "audit_oldvalue TEXT NULL ,";
    TriggerLite << "audit_newvalue TEXT NULL ,";
    TriggerLite << "audit_insdeldata TEXT NULL ,";
    TriggerLite << "PRIMARY KEY (audit_id) );";
    TriggerLite << "";

    sql = "SELECT table_name FROM information_schema.tables WHERE table_type = 'BASE TABLE' and TABLE_SCHEMA = '" + schema + "'"; //Excluse views in audit
    if (query.exec(sql))
    {
        while (query.next())
        {
            QUuid triggerUUID=QUuid::createUuid();
            QString strTriggerUUID=triggerUUID.toString().replace("{","").replace("}","").replace("-","_");
            if (ignoreTables.indexOf(query.value(0).toString().toLower()) < 0)
            {
                //Update trigger for MySQL-------------------------------------------------------------------
                dropMyTriggers << "DROP TRIGGER audit_" + strTriggerUUID +"_update;";
                TriggerData << "delimiter $$";
                TriggerData << "CREATE TRIGGER audit_" + strTriggerUUID +"_update";
                TriggerData << "AFTER UPDATE ON " + query.value(0).toString();
                TriggerData << "FOR EACH ROW BEGIN";
                TriggerData << "DECLARE ts TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6);";
                TriggerData << "DECLARE tuuid VARCHAR(64);";
                TriggerData << "SET tuuid = uuid();";
                TriggerData << "";

                QString mykeyData;
                mykeyData = "OLD.rowuuid";
                sql = "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + query.value(0).toString() + "' ORDER BY ORDINAL_POSITION";                
                if (query2.exec(sql))
                {
                    while (query2.next())
                    {
                        TriggerData << "IF OLD." + query2.value(0).toString() + " <> NEW." + query2.value(0).toString() + " THEN INSERT INTO audit_log(audit_id,audit_date,audit_action,audit_user,audit_table,audit_column,audit_key,audit_oldvalue,audit_newvalue) VALUES (tuuid,ts,'UPDATE',@odktools_current_user,'" + query.value(0).toString() + "','" + query2.value(0).toString() + "'," + mykeyData + ",OLD." + query2.value(0).toString() + ",NEW." + query2.value(0).toString() + ");";
                        TriggerData << "END IF;";
                    }
                }

                TriggerData << "";
                TriggerData << "END$$";
                TriggerData << "DELIMITER ;";
                TriggerData << "";

                //Insert trigger for MySQL-----------------------------------------------------------------------------------

                dropMyTriggers << "DROP TRIGGER audit_" + strTriggerUUID +"_insert;";

                TriggerData << "delimiter $$";
                TriggerData << "CREATE TRIGGER audit_" + strTriggerUUID +"_insert";
                TriggerData << "AFTER INSERT ON " + query.value(0).toString();
                TriggerData << "FOR EACH ROW BEGIN";
                TriggerData << "DECLARE ts TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6);";
                TriggerData << "DECLARE tuuid VARCHAR(64);";
                TriggerData << "SET tuuid = uuid();";
                TriggerData << "";
                TriggerData << "IF @odktools_ignore_insert IS NULL THEN ";


                mykeyData = "NEW.rowuuid";
                QString myNoKeyData;
                sql = "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + query.value(0).toString() + "' AND COLUMN_NAME != 'rowuuid'";                
                if (query2.exec(sql))
                {
                    myNoKeyData = "CONCAT(";
                    while (query2.next())
                    {
                        myNoKeyData = myNoKeyData + "'(" + query2.value(0).toString() + ")',ifnull(hex(cast(NEW." + query2.value(0).toString() + " as char)),''),',',";
                    }
                }
                if (myNoKeyData != "CONCAT(")
                    myNoKeyData = myNoKeyData.left(myNoKeyData.length()-5) + ")";
                else
                    myNoKeyData = "''";

                TriggerData << "INSERT INTO audit_log(audit_id,audit_date,audit_action,audit_user,audit_table,audit_key,audit_insdeldata) VALUES (tuuid,ts,'INSERT',@odktools_current_user,'" + query.value(0).toString() + "'," + mykeyData + "," + myNoKeyData + ");";
                TriggerData << "END IF;";
                TriggerData << "";
                TriggerData << "END$$";
                TriggerData << "DELIMITER ;";
                TriggerData << "";

                //Delete trigger for MySQL----------------------------------------------------------------------------

                dropMyTriggers << "DROP TRIGGER audit_" + strTriggerUUID +"_delete;";

                TriggerData << "delimiter $$";
                TriggerData << "CREATE TRIGGER audit_" + strTriggerUUID +"_delete";
                TriggerData << "AFTER DELETE ON " + query.value(0).toString();
                TriggerData << "FOR EACH ROW BEGIN";
                TriggerData << "DECLARE ts TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6);";
                TriggerData << "DECLARE tuuid VARCHAR(64);";
                TriggerData << "SET tuuid = uuid();";
                TriggerData << "";

                mykeyData = "OLD.rowuuid";
                sql = "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + query.value(0).toString() + "' AND COLUMN_NAME != 'rowuuid'";
                if (query2.exec(sql))
                {
                    myNoKeyData = "CONCAT(";
                    while (query2.next())
                    {
                        myNoKeyData = myNoKeyData + "'(" + query2.value(0).toString() + ")',ifnull(hex(cast(OLD." + query2.value(0).toString() + " as char)),''),',',";
                    }
                }
                if (myNoKeyData != "CONCAT(")
                    myNoKeyData = myNoKeyData.left(myNoKeyData.length()-5) + ")";
                else
                    myNoKeyData = "''";


                TriggerData << "INSERT INTO audit_log(audit_id,audit_date,audit_action,audit_user,audit_table,audit_key,audit_insdeldata) VALUES (tuuid,ts,'DELETE',@odktools_current_user,'" + query.value(0).toString() + "'," + mykeyData + "," + myNoKeyData + ");";

                TriggerData << "";
                TriggerData << "END$$";
                TriggerData << "DELIMITER ;";
                TriggerData << "";

 //----------------------------------------------------------------------SQLite------------------------------------------------------------------------------------------------------------------------------

                //Update trigger for SQLite------------------------------------------------------------------

                QString litekeyData;
                QString liteNoKeyData;

                litekeyData = "OLD.rowuuid";                
                sql = "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + query.value(0).toString() + "' ORDER BY ORDINAL_POSITION";
                if (query2.exec(sql))
                {
                    while (query2.next())
                    {
                        QUuid fieldtriggerUUID=QUuid::createUuid();
                        QString strfieldtriggerUUID=fieldtriggerUUID.toString().replace("{","").replace("}","").replace("-","_");

                        dropLiteTriggers << "DROP TRIGGER audit_" + strfieldtriggerUUID + "_update;";
                        TriggerLite << "CREATE TRIGGER audit_" + strfieldtriggerUUID + "_update";
                        TriggerLite << "AFTER UPDATE ON " + query.value(0).toString();
                        TriggerLite << "FOR EACH ROW ";
                        TriggerLite << "WHEN NEW." + query2.value(0).toString() + " != OLD." + query2.value(0).toString();
                        TriggerLite << "BEGIN ";
                        TriggerLite << "INSERT INTO audit_log(audit_id,audit_date,audit_action,audit_user,audit_table,audit_column,audit_key,audit_oldvalue,audit_newvalue) values ((hex( randomblob(4)) || '-' || hex( randomblob(2)) || '-' || '4' || substr( hex( randomblob(2)), 2) || '-' || substr('AB89', 1 + (abs(random()) % 4) , 1)  || substr(hex(randomblob(2)), 2) || '-' || hex(randomblob(6))),STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW'),'UPDATE','NONE','" + query.value(0).toString() + "','" + query2.value(0).toString() + "'," + litekeyData + ",OLD." + query2.value(0).toString() + ",NEW." + query2.value(0).toString() + ");";
                        TriggerLite << "END;";
                        TriggerLite << "";
                    }
                }
                TriggerLite << "";

                //Insert trigger for SQLite---------------------------------------------------------------------------                
                dropLiteTriggers << "DROP TRIGGER audit_" + strTriggerUUID + "_insert;";

                TriggerLite << "CREATE TRIGGER audit_" + strTriggerUUID + "_insert";
                TriggerLite << "AFTER INSERT ON " + query.value(0).toString();
                TriggerLite << "FOR EACH ROW BEGIN";
                TriggerLite << "";
                litekeyData = "NEW.rowuuid";

                sql = "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + query.value(0).toString() + + "' AND COLUMN_NAME != 'rowuuid'";
                if (query2.exec(sql))
                {
                    liteNoKeyData = "(";
                    while (query2.next())
                    {
                        liteNoKeyData = liteNoKeyData + "'(" + query2.value(0).toString() + ")' || hex(NEW." + query2.value(0).toString() + ") || ',' ||";
                    }
                }
                if (liteNoKeyData != "(")
                    liteNoKeyData = liteNoKeyData.left(liteNoKeyData.length()-9) + ")";
                else
                    liteNoKeyData = "''";

                TriggerLite << "INSERT INTO audit_log(audit_id,audit_date,audit_action,audit_user,audit_table,audit_key,audit_insdeldata) values ((hex( randomblob(4)) || '-' || hex( randomblob(2)) || '-' || '4' || substr( hex( randomblob(2)), 2) || '-' || substr('AB89', 1 + (abs(random()) % 4) , 1)  || substr(hex(randomblob(2)), 2) || '-' || hex(randomblob(6))),STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW'),'INSERT','NONE','" + query.value(0).toString() + "'," + litekeyData + "," + liteNoKeyData + ");";

                TriggerLite << "";
                TriggerLite << "END;";
                TriggerLite << "";

                //Delete trigger for SQLite----------------------------------------------------------------------------------

                dropLiteTriggers << "DROP TRIGGER audit_" + strTriggerUUID + "_delete;";

                TriggerLite << "CREATE TRIGGER audit_" + strTriggerUUID + "_delete";
                TriggerLite << "AFTER DELETE ON " + query.value(0).toString();
                TriggerLite << "FOR EACH ROW BEGIN";
                TriggerLite << "";
                litekeyData = "OLD.rowuuid";

                sql = "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + query.value(0).toString() + "' AND COLUMN_KEY != 'PRI'";
                if (query2.exec(sql))
                {
                    liteNoKeyData = "(";
                    while (query2.next())
                    {
                        liteNoKeyData = liteNoKeyData + "'(" + query2.value(0).toString() + ")' || hex(OLD." + query2.value(0).toString() + ") || ',' ||";
                    }
                }
                if (liteNoKeyData != "(")
                    liteNoKeyData = liteNoKeyData.left(liteNoKeyData.length()-9) + ")";
                else
                    liteNoKeyData = "''";

                TriggerLite << "INSERT INTO audit_log(audit_id,audit_date,audit_action,audit_user,audit_table,audit_key,audit_insdeldata) values ((hex( randomblob(4)) || '-' || hex( randomblob(2)) || '-' || '4' || substr( hex( randomblob(2)), 2) || '-' || substr('AB89', 1 + (abs(random()) % 4) , 1)  || substr(hex(randomblob(2)), 2) || '-' || hex(randomblob(6))),STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW'),'DELETE','NONE','" + query.value(0).toString() + "'," + litekeyData + "," + liteNoKeyData + ");";

                TriggerLite << "";
                TriggerLite << "END;";
                TriggerLite << "";
            }
        }

        TriggerLite << "CREATE TRIGGER noAuditUpdates BEFORE UPDATE ON audit_log FOR EACH ROW BEGIN SELECT CASE WHEN ((SELECT NULL) IS NULL) THEN RAISE(ABORT, 'Audit table cannot be updated') END; END;";
        TriggerLite << "";

        TriggerLite << "CREATE TRIGGER noAuditDeletes BEFORE DELETE ON audit_log FOR EACH ROW BEGIN SELECT CASE WHEN ((SELECT NULL) IS NULL) THEN RAISE(ABORT, 'Audit table cannot be deleted') END; END;";
        TriggerLite << "";

        TriggerLite << "COMMIT;";
        TriggerLite << "";

        QDir audir(auditDir);
        if (!audir.exists())
        {
            audir.setPath(".");
            if (!audir.mkdir(auditDir))
            {
                log("Error creating audit dir");
                return 1;
            }
            else
            {
                audir.cd(auditDir);
            }

        }

        //Saves the MySQL create audit file
        QString fileName;
        fileName = audir.absolutePath() + "/" + "mysql_create_audit.sql";
        QFileInfo f(fileName);
        QString fp;
        fp = f.path();
        if (!fileName.isEmpty())
        {
            QFile myfile(fileName);
            if (!myfile.open(QIODevice::WriteOnly | QIODevice::Text))
                return 1;

            QTextStream myout(&myfile);
            for (int pos = 0; pos <= TriggerData.count() -1;pos++)
                myout << TriggerData[pos] << "\n";
        }
        //log("MySQL Create audit script loaded in " + fileName);

        //Saves the SQLite create audit file
        fileName = "";
        fileName = audir.absolutePath() + "/" + "sqlite_create_audit.sql";

        if (!fileName.isEmpty())
        {
            QFile litefile(fileName);
            if (!litefile.open(QIODevice::WriteOnly | QIODevice::Text))
                return 1;

            QTextStream liteout(&litefile);
            for (int pos = 0; pos <= TriggerLite.count() -1;pos++)
                liteout << TriggerLite[pos] << "\n";
        }
        //log("SQLite Create audit script loaded in " + fileName);

        //Saves the MySQL drop audit file
        fileName = "";
        fileName = audir.absolutePath() + "/" + "mysql_drop_audit.sql";
        if (!fileName.isEmpty())
        {
            QFile mydropfile(fileName);
            if (!mydropfile.open(QIODevice::WriteOnly | QIODevice::Text))
                return 1;

            QTextStream mydropout(&mydropfile);
            for (int pos = 0; pos <= dropMyTriggers.count() -1;pos++)
                mydropout << dropMyTriggers[pos] << "\n";
        }
        //log("MySQL Drop audit script loaded in " + fileName);

        //Saves the SQlite drop audit file
        fileName = "";
        fileName = audir.absolutePath() + "/" + "sqlite_drop_audit.sql";
        if (!fileName.isEmpty())
        {
            QFile litedropfile(fileName);
            if (!litedropfile.open(QIODevice::WriteOnly | QIODevice::Text))
                return 1;

            QTextStream litedropout(&litedropfile);
            for (int pos = 0; pos <= dropLiteTriggers.count() -1;pos++)
                litedropout << dropLiteTriggers[pos] << "\n";
        }
        //log("SQLite Drop audit script loaded in " + fileName);
    }
    return 0;
}

void mainClass::run()
{
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
        db.setHostName(host);
        db.setPort(port.toInt());
        db.setDatabaseName(schema);
        db.setUserName(user);
        db.setPassword(pass);
        if (db.open())
        {
            QDir outDir(outputDirectory);
            if (!outDir.exists())
            {
                if (!outDir.mkdir(outputDirectory))
                {
                    log("Unable to create output directory");
                    returnCode = 1;
                }
            }
            QStringList ignore_table;
            ignore_table << "audit_log";
            ignore_table << "alembic_version";
            returnCode = createAudit(db, outputDirectory, ignore_table);
            db.close();
        }
        else
        {
            log("Unable to connect to MySQL");
            returnCode = 1;
        }
    }
    emit finished();
}
