#include "listmutex.h"


ListMutex::ListMutex(QObject *parent)
    : QObject{parent}
{

}

void ListMutex::set_total(int total)
{
    this->total = total-1;
}

int ListMutex::get_index()
{
    int res = -1;
    mutex.lock();
    res = total;
    total = total -1;
    mutex.unlock();
    return res;
}
