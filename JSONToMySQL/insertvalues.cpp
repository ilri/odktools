#include "insertvalues.h"


insertValues::insertValues(QObject *parent) : QObject(parent)
{

}

insertValues::~insertValues()
{

}

void insertValues::insertValue(TinsertValueDef value)
{
    m_insertList.append(value);
}

void insertValues::insertValue(QString name,QString xmlCode,QString value,bool isKey)
{
    TinsertValueDef newvalue;
    newvalue.name = name;
    newvalue.xmlCode = xmlCode;
    newvalue.value = value;
    newvalue.key = isKey;
    newvalue.insert = true;
    m_insertList.append(newvalue);
}

int insertValues::count()
{
    return m_insertList.count();
}

void insertValues::setItemName(int index, QString name)
{
    m_insertList[index].name = name;
}

void insertValues::setItemXMLCode(int index, QString xmlCode)
{
    m_insertList[index].xmlCode = xmlCode;
}

void insertValues::setItemValue(int index, QString value)
{
    m_insertList[index].value = value;
}

void insertValues::setItemIsKey(int index, bool isKey)
{
    m_insertList[index].key = isKey;
}

void insertValues::setItemToInsert(int index, bool toInsert)
{
    if (m_insertList[index].key == false)
        m_insertList[index].insert = toInsert; //Only change in the field is not key
}

QString insertValues::itemName(int index)
{
    return m_insertList[index].name;
}

QString insertValues::itemXMLCode(int index)
{
    return m_insertList[index].xmlCode;
}

QString insertValues::itemValue(int index)
{
    return m_insertList[index].value;
}

bool insertValues::itemIsKey(int index)
{
    return m_insertList[index].key;
}

bool insertValues::itemToInsert(int index)
{
    return m_insertList[index].insert;
}

bool insertValues::valueIsNumber(int index)
{
    QString value;
    value = m_insertList[index].value;
    QString numbers;
    numbers = "0123456789.";
    for (int pos = 0; pos <= value.length()-1;pos++)
    {
        if (numbers.indexOf(value[pos]) < 0 )
            return false;
    }
    return true;
}

int insertValues::getIndexByColumnName(QString name)
{
    for (int pos = 0; pos < m_insertList.count();pos++)
    {
        if (m_insertList[pos].name.toLower().simplified() == name.toLower().simplified())
            return pos;
    }
    return -1;
}
