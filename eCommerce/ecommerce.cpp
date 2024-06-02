#include "ecommerce.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <stdexcept>

/**
 * @brief Constructor for the eCommerce class.
 *
 * Initializes the context, subscriber, and pusher sockets.
 * Sets up the initial state, logs start message, initializes products,
 * sets up connections, and starts necessary threads.
 *
 * @param a Pointer to QCoreApplication instance.
 */
eCommerce::eCommerce(QCoreApplication *a)
    : context(1), subscriber(context, ZMQ_SUB), pusher(context, ZMQ_PUSH), running(true)
{
    srand(time(0));
    qCInfo(ecommercelog) << "eCommerce server starting...";

    initializeProducts();
    setupConnections();
    startThreads();
}

/**
 * @brief Destructor for the eCommerce class.
 *
 * Ensures the threads are stopped gracefully.
 */
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

/**
 * @brief Sets up connections to the message broker using ZeroMQ.
 *
 * Establishes subscriber and pusher connections and subscribes to the eCommerce topic.
 */
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

/**
 * @brief Starts the server and heartbeat threads for handling messages and sending heartbeats.
 */
void eCommerce::startThreads()
{
    serverThread = std::thread(&eCommerce::serverTask, this);
    heartbeatThread = std::thread(&eCommerce::heartbeatTask, this);
}

/**
 * @brief Main server task for receiving and handling messages.
 *
 * Continuously checks for incoming messages and processes them.
 */
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

/**
 * @brief Reconnects the subscriber and pusher to the message broker endpoints.
 */
void eCommerce::reconnect()
{
    subscriber.connect("tcp://benternet.pxl-ea-ict.be:24042");
    qCInfo(ecommercelog) << "Subscriber reconnected to endpoint.";
    pusher.connect("tcp://benternet.pxl-ea-ict.be:24041");
    qCInfo(ecommercelog) << "Pusher reconnected to endpoint.";
}

/**
 * @brief Heartbeat task for sending periodic heartbeat messages to the broker.
 */
void eCommerce::heartbeatTask()
{
    while (running)
    {
        sendHeartbeat();
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}

/**
 * @brief Initializes the list of available products.
 */
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

/**
 * @brief Sends a heartbeat message to the broker to indicate the server is alive.
 */
void eCommerce::sendHeartbeat()
{
    std::string heartbeat = "eCommerce?>keepalive>heartbeat>";
    pusher.send(zmq::buffer(heartbeat), zmq::send_flags::none);
    qCInfo(heartbeatlog) << "Sent heartbeat message.";
}

/**
 * @brief Receives a heartbeat message and logs it.
 */
void eCommerce::receiveHeartbeat()
{
    std::string heartbeat = "eCommerce!>heartbeat>pulse";
    pusher.send(zmq::buffer(heartbeat), zmq::send_flags::none);
    qCInfo(heartbeatlog) << "Received heartbeat message.";
}

/**
 * @brief Handles incoming messages and routes them to the appropriate command handlers.
 *
 * @param msg The received message to handle.
 */
void eCommerce::handleMessage(const std::string& msg)
{
    if (msg == "eCommerce?>keepalive>heartbeat>") {
        return; // Skip logging and handling for this specific message
    }

    qCInfo(ecommercelog) << "Handling message:" << msg.c_str();

    if (msg.find("eCommerce?>") != std::string::npos)
    {
        auto segments = splitMessage(msg, '>');
        if (segments.size() < 3) {
            qCWarning(ecommercelog) << "Invalid message format:" << msg.c_str();
            return;
        }

        std::string username = segments[1];
        std::string command = segments[2];
        handleCommand(username, command, segments);
    }
}

/**
 * @brief Splits a message string into segments based on a delimiter.
 *
 * @param msg The message to split.
 * @param delimiter The character used to split the message.
 * @return std::vector<std::string> A vector containing the split segments.
 */
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

/**
 * @brief Handles specific commands based on the message received.
 *
 * @param username The username associated with the command.
 * @param command The command to handle.
 * @param segments The message segments for the command.
 */
void eCommerce::handleCommand(const std::string& username, const std::string& command, const std::vector<std::string>& segments)
{
    if (command == "start")
    {
        sendResponse(username, "start", getWelcomeMessage());
    }
    else if (command == "help")
    {
        sendResponse(username, "help", getHelpMessage());
    }
    else if (command == "browseProducts")
    {
        sendResponse(username, "browseProducts", getBrowseProductsMessage());
    }
    else if (command == "addToCart" && segments.size() == 5)
    {
        handleAddToCart(username, segments);
    }
    else if (command == "clearCart")
    {
        handleClearCart(username);
    }
    else if (command == "viewCart")
    {
        sendResponse(username, "viewCart", viewCart(username));
    }
    else if (command == "checkout")
    {
        checkout(username);
    }
    else if (command == "pay")
    {
        pay(username);
    }
    else if (command == "viewOrders")
    {
        sendResponse(username, "viewOrders", viewOrders(username));
    }
    else if (command == "stop")
    {
        stop(username);
    }
    else if (command == "heartbeat")
    {
        receiveHeartbeat();
    }
    else if (command == "updateCartItem" && segments.size() == 5)
    {
        updateCartItem(username, segments);
    }
    else if (command == "cancelOrder")
    {
        cancelOrder(username);
    }
    else if (command == "removeItemFromCart" && segments.size() == 5)
    {
        removeItemFromCart(username, segments);
    }
    else
    {
        qCWarning(ecommercelog) << "Unknown command:" << command.c_str();
    }
}

/**
 * @brief Sends a response message to the specified user with a given command and message.
 *
 * @param username The username to send the response to.
 * @param command The command associated with the response.
 * @param message The response message to send.
 */
void eCommerce::sendResponse(const std::string& username, const std::string& command, const std::string& message)
{
    std::string responseTopic = "eCommerce!>" + username + ">" + command;
    pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
    pusher.send(zmq::buffer(message), zmq::send_flags::none);
    qCInfo(ecommercelog) << "Sent message to topic:" << responseTopic.c_str();
}

/**
 * @brief Handles the "addToCart" command by adding the specified product to the user's cart.
 *
 * @param username The username of the user adding to the cart.
 * @param segments The message segments containing the command details.
 */
void eCommerce::handleAddToCart(const std::string& username, const std::vector<std::string>& segments)
{
    try
    {
        int productId = std::stoi(segments[3]);
        int quantity = std::stoi(segments[4]);
        validateAddToCartInput(productId, quantity);
        if (products.find(productId) != products.end()) {
            addToCart(username, productId, quantity);
            sendResponse(username, "addToCart", "Added product " + std::to_string(productId) + " to cart with quantity " + std::to_string(quantity));
        } else {
            sendResponse(username, "addToCart", "Error: Product ID " + std::to_string(productId) + " does not exist.");
        }
    }
    catch (const std::exception& e)
    {
        sendResponse(username, "addToCart", "Error: " + std::string(e.what()));
    }
}

/**
 * @brief Validates the inputs for the "addToCart" command.
 *
 * @param productId The product ID to add to the cart.
 * @param quantity The quantity of the product to add.
 * @throws std::invalid_argument if the quantity is not greater than zero.
 */
void eCommerce::validateAddToCartInput(int productId, int quantity)
{
    if (quantity <= 0) {
        throw std::invalid_argument("Quantity must be greater than zero.");
    }
}

/**
 * @brief Handles the "clearCart" command by clearing the user's cart.
 *
 * @param username The username of the user whose cart is to be cleared.
 */
void eCommerce::handleClearCart(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (userCarts.find(username) == userCarts.end() || userCarts[username].empty())
    {
        sendResponse(username, "clearCart", "Error: Your cart is already empty.");
    }
    else
    {
        userCarts.erase(username);
        sendResponse(username, "clearCart", "Your cart has been cleared.");
    }
}

/**
 * @brief Generates the help message listing available commands.
 *
 * @return std::string The help message.
 */
std::string eCommerce::getHelpMessage()
{
    return "Available commands:\n"
           "1. browseProducts - Display a list of available products.\n"
           "2. addToCart <productId> <quantity> - Add a product to the shopping cart.\n"
           "3. clearCart - Clear the entire shopping cart.\n"
           "4. viewCart - View the contents of the shopping cart.\n"
           "5. checkout - Process the checkout and place an order.\n"
           "6. pay - Complete the payment for your order.\n"
           "7. viewOrders - View past orders.\n"
           "8. stop - Log out and clear the cart.\n"
           "9. updateCartItem <productId> <quantity> - Update the quantity of a product in the shopping cart.\n"
           "10. cancelOrder - Cancel orders placed in the checkout but not yet paid.\n"
           "11. removeItemFromCart <productId> <quantity> - Remove a specified quantity of a product from the cart.\n";
}

/**
 * @brief Generates the welcome message for new users.
 *
 * @return std::string The welcome message.
 */
std::string eCommerce::getWelcomeMessage()
{
    return "Welcome to the eCommerce system!\n"
           "We are delighted to have you on board.\n"
           "This system allows you to browse products, add them to your cart, and make purchases with ease.\n"
           "To get started, you can use the following commands:\n"
           "1. browseProducts - Display a list of available products.\n"
           "2. addToCart <productId> <quantity> - Add a product to the shopping cart.\n"
           "3. clearCart - Clear the entire shopping cart.\n"
           "4. viewCart - View the contents of the shopping cart.\n"
           "5. checkout - Process the checkout and place an order.\n"
           "6. pay - Complete the payment for your order.\n"
           "7. viewOrders - View past orders.\n"
           "8. stop - Log out and clear the cart.\n"
           "9. updateCartItem <productId> <quantity> - Update the quantity of a product in the shopping cart.\n"
           "10. cancelOrder - Cancel orders placed in the checkout but not yet paid.\n"
           "11. removeItemFromCart <productId> <quantity> - Remove a specified quantity of a product from the cart.\n"
           "If you need any assistance, please use the 'help' command or contact our support team.\n"
           "Happy shopping!";
}

/**
 * @brief Generates the message listing available products.
 *
 * @return std::string The message listing available products.
 */
std::string eCommerce::getBrowseProductsMessage()
{
    std::string productsMsg = "Available products:\n";
    for (const auto& product : products) {
        productsMsg += std::to_string(product.first) + ". " + product.second.first + " - $" + std::to_string(product.second.second) + "\n";
    }
    return productsMsg;
}

/**
 * @brief Adds a product to the user's cart.
 *
 * @param username The username of the user adding to the cart.
 * @param productId The product ID to add to the cart.
 * @param quantity The quantity of the product to add.
 */
void eCommerce::addToCart(const std::string& username, int productId, int quantity)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (products.find(productId) != products.end()) {
        userCarts[username][productId] += quantity;
    }
}

/**
 * @brief Views the contents of the user's cart.
 *
 * @param username The username of the user viewing the cart.
 * @return std::string The message containing the cart contents.
 */
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

/**
 * @brief Handles the "checkout" command by placing an order for the user.
 *
 * @param username The username of the user placing the order.
 */
void eCommerce::checkout(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (userCarts.find(username) != userCarts.end() && !userCarts[username].empty()) {
        orders[username].push_back(userCarts[username]);
        userCarts.erase(username);
        userPaymentStatus[username] = false;
        sendResponse(username, "checkout", "Your order has been placed successfully. Please proceed to payment.");
    } else {
        sendResponse(username, "checkout", "Your cart is empty. Cannot place an order.");
    }
}

/**
 * @brief Handles the "pay" command by completing the payment for the user's order.
 *
 * @param username The username of the user making the payment.
 */
void eCommerce::pay(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (orders.find(username) != orders.end() && !orders[username].empty())
    {
        userPaymentStatus[username] = true;
        sendResponse(username, "pay", "Your payment has been received. Thank you for your purchase!");
    }
    else
    {
        sendResponse(username, "pay", "No pending orders to pay for.");
    }
}

/**
 * @brief Views the past orders for the user.
 *
 * @param username The username of the user viewing orders.
 * @return std::string The message containing the past orders.
 */
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

/**
 * @brief Handles the "stop" command by logging out the user and clearing their cart.
 *
 * @param username The username of the user logging out.
 */
void eCommerce::stop(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    userCarts.erase(username);
    userPaymentStatus.erase(username);
    sendResponse(username, "stop", "User " + username + " has been logged out and their cart has been cleared.");
}

/**
 * @brief Handles the "updateCartItem" command by updating the quantity of a product in the user's cart.
 *
 * @param username The username of the user updating the cart.
 * @param segments The message segments containing the command details.
 */
void eCommerce::updateCartItem(const std::string& username, const std::vector<std::string>& segments)
{
    try
    {
        int productId = std::stoi(segments[3]);
        int quantity = std::stoi(segments[4]);
        validateAddToCartInput(productId, quantity);
        std::lock_guard<std::mutex> lock(cartMutex);
        if (products.find(productId) != products.end() && userCarts[username].find(productId) != userCarts[username].end()) {
            userCarts[username][productId] = quantity;
            sendResponse(username, "updateCartItem", "Updated product " + std::to_string(productId) + " to quantity " + std::to_string(quantity));
        } else {
            sendResponse(username, "updateCartItem", "Error: Product ID " + std::to_string(productId) + " does not exist in your cart.");
        }
    }
    catch (const std::exception& e)
    {
        sendResponse(username, "updateCartItem", "Error: " + std::string(e.what()));
    }
}

/**
 * @brief Handles the "cancelOrder" command by cancelling orders placed in the checkout but not yet paid.
 *
 * @param username The username of the user cancelling the order.
 */
void eCommerce::cancelOrder(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (orders.find(username) != orders.end() && !orders[username].empty() && !userPaymentStatus[username]) {
        orders[username].pop_back();
        sendResponse(username, "cancelOrder", "Your last order has been cancelled.");
    } else {
        sendResponse(username, "cancelOrder", "No orders to cancel or the order has already been paid.");
    }
}

/**
 * @brief Handles the "removeItemFromCart" command by removing a specified quantity of a product from the user's cart.
 *
 * @param username The username of the user removing the item from the cart.
 * @param segments The message segments containing the command details.
 */
void eCommerce::removeItemFromCart(const std::string& username, const std::vector<std::string>& segments)
{
    try
    {
        int productId = std::stoi(segments[3]);
        int quantity = std::stoi(segments[4]);
        std::lock_guard<std::mutex> lock(cartMutex);
        if (userCarts[username].find(productId) != userCarts[username].end()) {
            if (userCarts[username][productId] >= quantity) {
                userCarts[username][productId] -= quantity;
                if (userCarts[username][productId] == 0) {
                    userCarts[username].erase(productId);
                }
                sendResponse(username, "removeItemFromCart", "Removed " + std::to_string(quantity) + " of product " + std::to_string(productId) + " from cart.");
            } else {
                sendResponse(username, "removeItemFromCart", "Error: Not enough quantity to remove.");
            }
        } else {
            sendResponse(username, "removeItemFromCart", "Error: Product ID " + std::to_string(productId) + " does not exist in your cart.");
        }
    }
    catch (const std::exception& e)
    {
        sendResponse(username, "removeItemFromCart", "Error: " + std::string(e.what()));
    }
}
