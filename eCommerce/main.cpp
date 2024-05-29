// File: main.cpp

#include <QCoreApplication>
#include "ecommerce.h"
#include "loggingcategories.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Initial logging filter rules
    QLoggingCategory::setFilterRules("ecommerce.debug=false\n"
                                     "ecommerce.info=true\n"
                                     "ecommerce.warning=true\n"
                                     "ecommerce.critical=true\n"
                                     "heartbeat.info=false");

    eCommerce *ecommerce = new eCommerce(&a);

    return a.exec();
}
