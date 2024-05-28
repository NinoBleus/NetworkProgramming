#include <QCoreApplication>
#include "ecommerce.h"
#include "loggingcategories.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QLoggingCategory::setFilterRules("ecommerce.debug=false\n"
                                     "ecommerce.info=false\n"
                                     "ecommerce.warning=true\n"
                                     "ecommerce.critical=true");

    eCommerce *ecommerce = new eCommerce(&a);
    return a.exec();
}
