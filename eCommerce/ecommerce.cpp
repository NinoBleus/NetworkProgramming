#include "ecommerce.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <iostream>

eCommerce::eCommerce(QCoreApplication *a)
    : context(1), subscriber(context, ZMQ_SUB), pusher(context, ZMQ_PUSH), running(true)
{
    srand(time(0));
    qCInfo(ecommercelog) << "eCommerce server starting...";
    initializeProducts();
    setupConnections();
    startThreads();
}

eCommerce::~eCommerce()
{
    running = false;
    if (serverThread.joinable()) {
        serverThread.join();
    }
    if (heartbeatThread.joinable()) {
        heartbeatThread.join();
    }
}

void eCommerce::setupConnections()
{
    try
    {
        subscriber.connect("tcp://benternet.pxl-ea-ict.be:24042");
        qCInfo(ecommercelog) << "Subscriber connected to endpoint.";
        pusher.connect("tcp://benternet.pxl-ea-ict.be:24041");
        qCInfo(ecommercelog) << "Pusher connected to endpoint.";

        const char* topic = "eCommerce?";
        subscriber.set(zmq::sockopt::subscribe, topic);
        subscriber.set(zmq::sockopt::rcvtimeo, 1000);
        qCInfo(ecommercelog) << "Subscribed to topic:" << topic;
    }
    catch (zmq::error_t& ex)
    {
        qCCritical(ecommercelog) << "Caught an exception:" << ex.what();
    }
}

void eCommerce::startThreads()
{
    serverThread = std::thread(&eCommerce::serverTask, this);
    heartbeatThread = std::thread(&eCommerce::heartbeatTask, this);
}

void eCommerce::serverTask()
{
    while (running)
    {
        try
        {
            zmq::message_t msg;
            if (subscriber.recv(msg, zmq::recv_flags::dontwait))
            {
                std::string receivedMsg(static_cast<char*>(msg.data()), msg.size());
                qCInfo(ecommercelog) << "Subscriber received:" << receivedMsg.c_str();

                if (receivedMsg.find("eCommerce!>") == std::string::npos) {
                    handleMessage(receivedMsg);
                }
            }
        }
        catch (zmq::error_t& ex)
        {
            qCCritical(ecommercelog) << "Caught an exception:" << ex.what();
            reconnect();
        }
        catch (const std::exception& e)
        {
            qCCritical(ecommercelog) << "Caught an exception:" << e.what();
        }
    }
}

void eCommerce::reconnect()
{
    subscriber.connect("tcp://benternet.pxl-ea-ict.be:24042");
    qCInfo(ecommercelog) << "Subscriber reconnected to endpoint.";
    pusher.connect("tcp://benternet.pxl-ea-ict.be:24041");
    qCInfo(ecommercelog) << "Pusher reconnected to endpoint.";
}

void eCommerce::heartbeatTask()
{
    while (running)
    {
        sendHeartbeat();
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}

void eCommerce::initializeProducts()
{
    products = {
        {1, {"Apple iPhone 13", 799.00}},
        {2, {"Samsung Galaxy S21", 699.00}},
        {3, {"Sony WH-1000XM4 Headphones", 349.00}},
        {4, {"Apple MacBook Pro 14\"", 1999.00}},
        {5, {"Dell XPS 13 Laptop", 999.00}},
        {6, {"Nintendo Switch", 299.00}},
        {7, {"Amazon Echo Dot (4th Gen)", 49.99}},
        {8, {"Fitbit Charge 5", 179.95}},
        {9, {"Instant Pot Duo 7-in-1", 89.00}},
        {10, {"Sony PlayStation 5", 499.00}}
    };
}

void eCommerce::sendHeartbeat()
{
    std::string heartbeat = "eCommerce?>keepalive>heartbeat>";
    pusher.send(zmq::buffer(heartbeat), zmq::send_flags::none);
    qCInfo(heartbeatlog) << "Sent heartbeat message.";
}

void eCommerce::receiveHeartbeat()
{
    std::string heartbeat = "eCommerce!>heartbeat>pulse";
    pusher.send(zmq::buffer(heartbeat), zmq::send_flags::none);
    qCInfo(heartbeatlog) << "Received heartbeat message.";
}

void eCommerce::handleMessage(const std::string& msg)
{
    qCInfo(ecommercelog) << "Handling message:" << msg.c_str();

    auto segments = splitMessage(msg, '>');
    if (segments.size() < 4) {
        qCWarning(ecommercelog) << "Invalid message format:" << msg.c_str();
        return;
    }

    std::string username = segments[1];
    std::string command = segments[2];
    std::string password = segments[3];

    if (command == "start") {
        if (segments.size() != 4) {
            qCWarning(ecommercelog) << "Invalid start command format:" << msg.c_str();
            return;
        }
        qCInfo(ecommercelog) << "Setting password for user: " << username;
        setUserPassword(username, password);
        sendResponse(username, "start", getWelcomeMessage(), password);
        return;
    }

    qCInfo(ecommercelog) << "Verifying password for user: " << username;
    if (!verifyUserPassword(username, password)) {
        sendResponse(username, command, "Error: Incorrect password.", password);
        return;
    }

    if (command == "keepalive") {
        return; // Skip logging and handling for this specific message
    }

    if (command == "browseProducts") {
        sendResponse(username, "browseProducts", getBrowseProductsMessage(), password);
    }
    else if (command == "addToCart" && segments.size() == 6)
    {
        handleAddToCart(username, segments, password);
    }
    else if (command == "clearCart")
    {
        handleClearCart(username, password);
    }
    else if (command == "viewCart")
    {
        sendResponse(username, "viewCart", viewCart(username), password);
    }
    else if (command == "checkout")
    {
        checkout(username, password);
    }
    else if (command == "pay")
    {
        pay(username, password);
    }
    else if (command == "viewOrders")
    {
        sendResponse(username, "viewOrders", viewOrders(username), password);
    }
    else if (command == "stop")
    {
        stop(username, password);
    }
    else if (command == "heartbeat")
    {
        receiveHeartbeat();
    }
    else if (command == "updateCartItem" && segments.size() == 6)
    {
        updateCartItem(username, segments, password);
    }
    else if (command == "cancelOrder")
    {
        cancelOrder(username, password);
    }
    else if (command == "removeItemFromCart" && segments.size() == 6)
    {
        removeItemFromCart(username, segments, password);
    }
    else if (command == "addToWishlist" && segments.size() == 5)
    {
        handleAddToWishlist(username, segments, password);
    }
    else if (command == "removeFromWishlist" && segments.size() == 5)
    {
        handleRemoveFromWishlist(username, segments, password);
    }
    else
    {
        qCWarning(ecommercelog) << "Unknown command:" << command.c_str();
    }
}

std::vector<std::string> eCommerce::splitMessage(const std::string& msg, char delimiter)
{
    std::stringstream ss(msg);
    std::string segment;
    std::vector<std::string> segments;
    while (std::getline(ss, segment, delimiter))
    {
        segments.push_back(segment);
    }
    return segments;
}

void eCommerce::setUserPassword(const std::string& username, const std::string& password)
{
    std::lock_guard<std::mutex> lock(passwordMutex);
    qCInfo(ecommercelog) << "Setting password for user: " << username << " Password: " << password;
    userPasswords[username] = password;
}

bool eCommerce::verifyUserPassword(const std::string& username, const std::string& password)
{
    std::lock_guard<std::mutex> lock(passwordMutex);
    if (userPasswords.find(username) == userPasswords.end()) {
        qCInfo(ecommercelog) << "Password verification failed: User not found";
        return false;
    }
    qCInfo(ecommercelog) << "Stored password: " << userPasswords[username] << " Provided password: " << password;
    return userPasswords[username] == password;
}

void eCommerce::handleAddToCart(const std::string& username, const std::vector<std::string>& segments, const std::string& password)
{
    try
    {
        int productId = std::stoi(segments[4]);
        int quantity = std::stoi(segments[5]);
        validateAddToCartInput(productId, quantity);
        if (products.find(productId) != products.end()) {
            addToCart(username, productId, quantity);
            sendResponse(username, "addToCart", "Added product " + std::to_string(productId) + " to cart with quantity " + std::to_string(quantity), password);
        } else {
            sendResponse(username, "addToCart", "Error: Product ID " + std::to_string(productId) + " does not exist.", password);
        }
    }
    catch (const std::exception& e)
    {
        sendResponse(username, "addToCart", "Error: " + std::string(e.what()), password);
    }
}

void eCommerce::validateAddToCartInput(int productId, int quantity)
{
    if (quantity <= 0) {
        throw std::invalid_argument("Quantity must be greater than zero.");
    }
}

void eCommerce::handleClearCart(const std::string& username, const std::string& password)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (userCarts.find(username) == userCarts.end() || userCarts[username].empty())
    {
        sendResponse(username, "clearCart", "Error: Your cart is already empty.", password);
    }
    else
    {
        userCarts.erase(username);
        sendResponse(username, "clearCart", "Your cart has been cleared.", password);
    }
}

std::string eCommerce::getHelpMessage()
{
    return "Available commands:\n"
           "1. browseProducts <password> - Display a list of available products.\n"
           "2. addToCart <password> <productId> <quantity> - Add a product to the shopping cart.\n"
           "3. clearCart <password> - Clear the entire shopping cart.\n"
           "4. viewCart <password> - View the contents of the shopping cart.\n"
           "5. checkout <password> - Process the checkout and place an order.\n"
           "6. pay <password> - Complete the payment for your order.\n"
           "7. viewOrders <password> - View past orders.\n"
           "8. stop <password> - Log out and clear the cart.\n"
           "9. updateCartItem <password> <productId> <quantity> - Update the quantity of a product in the shopping cart.\n"
           "10. cancelOrder <password> - Cancel orders placed in the checkout but not yet paid.\n"
           "11. removeItemFromCart <password> <productId> <quantity> - Remove a specified quantity of a product from the cart.\n"
           "12. addToWishlist <password> <productId> - Add a product to the wishlist.\n"
           "13. removeFromWishlist <password> <productId> - Remove a product from the wishlist.\n";
}

std::string eCommerce::getWelcomeMessage()
{
    return "Welcome to the eCommerce system!\n"
           "We are delighted to have you on board.\n"
           "This system allows you to browse products, add them to your cart, and make purchases with ease.\n"
           "To get started, you can use the following commands:\n"
           "1. browseProducts <password> - Display a list of available products.\n"
           "2. addToCart <password> <productId> <quantity> - Add a product to the shopping cart.\n"
           "3. clearCart <password> - Clear the entire shopping cart.\n"
           "4. viewCart <password> - View the contents of the shopping cart.\n"
           "5. checkout <password> - Process the checkout and place an order.\n"
           "6. pay <password> - Complete the payment for your order.\n"
           "7. viewOrders <password> - View past orders.\n"
           "8. stop <password> - Log out and clear the cart.\n"
           "9. updateCartItem <password> <productId> <quantity> - Update the quantity of a product in the shopping cart.\n"
           "10. cancelOrder <password> - Cancel orders placed in the checkout but not yet paid.\n"
           "11. removeItemFromCart <password> <productId> <quantity> - Remove a specified quantity of a product from the cart.\n"
           "12. addToWishlist <password> <productId> - Add a product to the wishlist.\n"
           "13. removeFromWishlist <password> <productId> - Remove a product from the wishlist.\n"
           "If you need any assistance, please use the 'help' command or contact our support team.\n"
           "Happy shopping!";
}

std::string eCommerce::getBrowseProductsMessage()
{
    std::string productsMsg = "Available products:\n";
    for (const auto& product : products) {
        productsMsg += std::to_string(product.first) + ". " + product.second.first + " - $" + std::to_string(product.second.second) + "\n";
    }
    return productsMsg;
}

void eCommerce::addToCart(const std::string& username, int productId, int quantity)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (products.find(productId) != products.end()) {
        userCarts[username][productId] += quantity;
    }
}

std::string eCommerce::viewCart(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    std::string cartMsg = "Cart contents for " + username + ":\n";
    double total = 0.0;
    if (userCarts.find(username) != userCarts.end()) {
        for (const auto& item : userCarts[username]) {
            cartMsg += products[item.first].first + " - Quantity: " + std::to_string(item.second) + " - $" + std::to_string(products[item.first].second * item.second) + "\n";
            total += products[item.first].second * item.second;
        }
    }
    cartMsg += "Total: $" + std::to_string(total) + "\n";
    return cartMsg;
}

void eCommerce::checkout(const std::string& username, const std::string& password)
{
    std::lock_guard<std::mutex> lock(cartMutex);

    if (userCarts.find(username) == userCarts.end() || userCarts[username].empty()) {
        if (userWishlists.find(username) != userWishlists.end() && !userWishlists[username].empty()) {
            for (const auto& productId : userWishlists[username]) {
                userCarts[username][productId] = 1;
            }
            userWishlists.erase(username);
        }
    }

    if (userCarts.find(username) != userCarts.end() && !userCarts[username].empty()) {
        std::string wishlistMsg = checkWishlist(username);
        if (!wishlistMsg.empty()) {
            sendResponse(username, "checkout", "You have items in your wishlist that are not in your cart:\n" + wishlistMsg, password);
            return;
        }
        orders[username].push_back(userCarts[username]);
        userCarts.erase(username);
        userPaymentStatus[username] = false;
        sendResponse(username, "checkout", "Your order has been placed successfully. Please proceed to payment.", password);
    } else {
        sendResponse(username, "checkout", "Your cart is empty. Cannot place an order.", password);
    }
}

void eCommerce::pay(const std::string& username, const std::string& password)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (orders.find(username) != orders.end() && !orders[username].empty())
    {
        userPaymentStatus[username] = true;
        sendResponse(username, "pay", "Your payment has been received. Thank you for your purchase!", password);
    }
    else
    {
        sendResponse(username, "pay", "No pending orders to pay for.", password);
    }
}

std::string eCommerce::viewOrders(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    std::string ordersMsg = "Past orders for " + username + ":\n";
    if (orders.find(username) != orders.end()) {
        int orderNumber = 1;
        for (const auto& order : orders[username]) {
            ordersMsg += "Order " + std::to_string(orderNumber++) + ":\n";
            double total = 0.0;
            for (const auto& item : order) {
                ordersMsg += products[item.first].first + " - Quantity: " + std::to_string(item.second) + " - $" + std::to_string(products[item.first].second * item.second) + "\n";
                total += products[item.first].second * item.second;
            }
            ordersMsg += "Total: $" + std::to_string(total) + "\n";
            ordersMsg += "Payment Status: " + std::string(userPaymentStatus[username] ? "Paid" : "Pending") + "\n";
        }
    } else {
        ordersMsg += "No orders found.\n";
    }
    return ordersMsg;
}

void eCommerce::stop(const std::string& username, const std::string& password)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    userCarts.erase(username);
    userPaymentStatus.erase(username);
    sendResponse(username, "stop", "User " + username + " has been logged out and their cart has been cleared.", password);
}

void eCommerce::updateCartItem(const std::string& username, const std::vector<std::string>& segments, const std::string& password)
{
    try
    {
        int productId = std::stoi(segments[4]);
        int quantity = std::stoi(segments[5]);
        validateAddToCartInput(productId, quantity);
        std::lock_guard<std::mutex> lock(cartMutex);
        if (products.find(productId) != products.end() && userCarts[username].find(productId) != userCarts[username].end()) {
            userCarts[username][productId] = quantity;
            sendResponse(username, "updateCartItem", "Updated product " + std::to_string(productId) + " to quantity " + std::to_string(quantity), password);
        } else {
            sendResponse(username, "updateCartItem", "Error: Product ID " + std::to_string(productId) + " does not exist in your cart.", password);
        }
    }
    catch (const std::exception& e)
    {
        sendResponse(username, "updateCartItem", "Error: " + std::string(e.what()), password);
    }
}

void eCommerce::cancelOrder(const std::string& username, const std::string& password)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (orders.find(username) != orders.end() && !orders[username].empty() && !userPaymentStatus[username]) {
        orders[username].pop_back();
        sendResponse(username, "cancelOrder", "Your last order has been cancelled.", password);
    } else {
        sendResponse(username, "cancelOrder", "No orders to cancel or the order has already been paid.", password);
    }
}

void eCommerce::removeItemFromCart(const std::string& username, const std::vector<std::string>& segments, const std::string& password)
{
    try
    {
        int productId = std::stoi(segments[4]);
        int quantity = std::stoi(segments[5]);
        std::lock_guard<std::mutex> lock(cartMutex);
        if (userCarts[username].find(productId) != userCarts[username].end()) {
            if (userCarts[username][productId] >= quantity) {
                userCarts[username][productId] -= quantity;
                if (userCarts[username][productId] == 0) {
                    userCarts[username].erase(productId);
                }
                sendResponse(username, "removeItemFromCart", "Removed " + std::to_string(quantity) + " of product " + std::to_string(productId) + " from cart.", password);
            } else {
                sendResponse(username, "removeItemFromCart", "Error: Not enough quantity to remove.", password);
            }
        } else {
            sendResponse(username, "removeItemFromCart", "Error: Product ID " + std::to_string(productId) + " does not exist in your cart.", password);
        }
    }
    catch (const std::exception& e)
    {
        sendResponse(username, "removeItemFromCart", "Error: " + std::string(e.what()), password);
    }
}

void eCommerce::handleAddToWishlist(const std::string& username, const std::vector<std::string>& segments, const std::string& password)
{
    try
    {
        int productId = std::stoi(segments[4]);
        std::lock_guard<std::mutex> lock(wishlistMutex);
        if (products.find(productId) != products.end()) {
            userWishlists[username].insert(productId);
            sendResponse(username, "addToWishlist", "Added product " + std::to_string(productId) + " to wishlist.", password);
        } else {
            sendResponse(username, "addToWishlist", "Error: Product ID " + std::to_string(productId) + " does not exist.", password);
        }
    }
    catch (const std::exception& e)
    {
        sendResponse(username, "addToWishlist", "Error: " + std::string(e.what()), password);
    }
}

void eCommerce::handleRemoveFromWishlist(const std::string& username, const std::vector<std::string>& segments, const std::string& password)
{
    try
    {
        int productId = std::stoi(segments[4]);
        std::lock_guard<std::mutex> lock(wishlistMutex);
        if (products.find(productId) != products.end()) {
            if (userWishlists[username].erase(productId)) {
                sendResponse(username, "removeFromWishlist", "Removed product " + std::to_string(productId) + " from wishlist.", password);
            } else {
                sendResponse(username, "removeFromWishlist", "Error: Product ID " + std::to_string(productId) + " does not exist in your wishlist.", password);
            }
        } else {
            sendResponse(username, "removeFromWishlist", "Error: Product ID " + std::to_string(productId) + " does not exist.", password);
        }
    }
    catch (const std::exception& e)
    {
        sendResponse(username, "removeFromWishlist", "Error: " + std::string(e.what()), password);
    }
}

std::string eCommerce::checkWishlist(const std::string& username)
{
    std::lock_guard<std::mutex> lock(wishlistMutex);
    std::string wishlistMsg;
    if (userWishlists.find(username) != userWishlists.end()) {
        for (int productId : userWishlists[username]) {
            if (userCarts[username].find(productId) == userCarts[username].end()) {
                wishlistMsg += products[productId].first + "\n";
            }
        }
    }
    return wishlistMsg;
}

/**
 * @brief Sends a response message to the client.
 *
 * @param username The username of the client.
 * @param command The command being responded to.
 * @param message The response message content.
 * @param password The password of the user for verification.
 */
void eCommerce::sendResponse(const std::string& username, const std::string& command, const std::string& message, const std::string& password)
{
    std::string response = "eCommerce!>" + username + ">" + command + ">" + password + ">" + message;
    pusher.send(zmq::buffer(response), zmq::send_flags::none);
}
