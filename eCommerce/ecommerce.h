#ifndef ECOMMERCE_H
#define ECOMMERCE_H

#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <zmq.hpp>
#include <QCoreApplication>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(ecommercelog)
Q_DECLARE_LOGGING_CATEGORY(heartbeatlog)

class eCommerce {
public:
    eCommerce(QCoreApplication *a);
    ~eCommerce();

    void sendHeartbeat();
    void receiveHeartbeat();

private:
    zmq::context_t context;
    zmq::socket_t subscriber;
    zmq::socket_t pusher;

    std::map<int, std::pair<std::string, double>> products;
    std::map<std::string, std::map<int, int>> userCarts;
    std::map<std::string, std::vector<std::map<int, int>>> orders;
    std::map<std::string, bool> userPaymentStatus;
    std::mutex cartMutex;

    std::thread serverThread;
    std::thread heartbeatThread;
    std::atomic<bool> running;

    void serverTask();
    void heartbeatTask();
    void handleMessage(const std::string& msg);
    void handleCommand(const std::string& username, const std::string& command, const std::vector<std::string>& segments);

    void sendResponse(const std::string& username, const std::string& command, const std::string& message);
    void handleAddToCart(const std::string& username, const std::vector<std::string>& segments);
    void handleClearCart(const std::string& username);
    void updateCartItem(const std::string& username, const std::vector<std::string>& segments);
    void cancelOrder(const std::string& username);
    void removeItemFromCart(const std::string& username, const std::vector<std::string>& segments);

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

    void initializeProducts();
    void setupConnections();
    void startThreads();
    void reconnect();

    std::vector<std::string> splitMessage(const std::string& msg, char delimiter);
    void validateAddToCartInput(int productId, int quantity);
};

#endif // ECOMMERCE_H
