#ifndef ECOMMERCE_H
#define ECOMMERCE_H

#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <zmq.hpp>
#include <QCoreApplication>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(ecommercelog)

class eCommerce {
public:
    eCommerce(QCoreApplication *a);

    void handleMessage(const std::string& msg);

private:
    zmq::context_t context;
    zmq::socket_t subscriber;
    zmq::socket_t pusher;

    std::map<int, std::pair<std::string, double>> products;
    std::map<std::string, std::map<int, int>> userCarts;
    std::map<std::string, std::vector<std::map<int, int>>> orders;
    std::map<std::string, bool> userPaymentStatus;
    std::mutex cartMutex;

    std::string getHelpMessage();
    std::string getWelcomeMessage();
    std::string getBrowseProductsMessage();
    std::string viewCart(const std::string& username);
    std::string viewOrders(const std::string& username);
    void addToCart(const std::string& username, int productId, int quantity);
    void removeFromCart(const std::string& username, int productId);
    bool canRemoveFromCart(const std::string& username, int productId);
    void checkout(const std::string& username);
    void stop(const std::string& username);
    void pay(const std::string& username);
    void sendHeartbeat();
    void receiveHeartbeat();
    void Log(const QString& message, QtMsgType type = QtInfoMsg);
};

#endif // ECOMMERCE_H
