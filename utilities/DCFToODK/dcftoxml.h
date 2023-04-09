#ifndef DCFTOXML_H
#define DCFTOXML_H

#include <QObject>
#include <QDomDocument>

class DCFToXML
{
public:
    DCFToXML();
    void convertToXML(QString dcfFile);
    QDomDocument xmlDoc;
private:
    void appendValue(QDomElement parent, QString data);
    void appendVar(QDomElement parent, QString data);
    void addToXML(QString data);
};

#endif // DCFTOXML_H
