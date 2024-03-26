#include <QCoreApplication>
#include "ecommerce.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    eCommerce *ecommerce = new eCommerce(&a);
    return a.exec();
}
