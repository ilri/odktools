#ifndef XMLTOYML_H
#define XMLTOYML_H

#include <QObject>
#include <QDomDocument>

struct lkpValue
{
    QString code;
    QString desc;
};
typedef lkpValue TlkpValue;

class XMLToYML : public QObject
{
    Q_OBJECT
public:
    explicit XMLToYML(QObject *parent = nullptr);
    void setXMLDocument(QDomDocument doc);
    void generateYML(QString file, QString mainRecord, QString tempDir);
private:
    QDomDocument xmlDoc;
    void addValueToList(QList<TlkpValue > &list, QString code, QString desc);
    void log(QString message);
};

#endif // XMLTOYML_H
