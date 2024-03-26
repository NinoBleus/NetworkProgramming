#ifndef ECOMMERCE_H
#define ECOMMERCE_H

#include <QCoreApplication>
#include <QObject>
#include "zmq.hpp"

class eCommerce: public QObject
{
public:
    eCommerce(QCoreApplication *a);
    void start(const std::vector<std::string>& messages);
    void help();
    void stop();


private:
    zmq::context_t context;
    zmq::socket_t subscriber;
    zmq::socket_t pusher;
};

#endif // ECOMMERCE_H
