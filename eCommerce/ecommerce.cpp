#include "ecommerce.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <stdexcept>

eCommerce::eCommerce(QCoreApplication *a)
    : context(1), subscriber(context, ZMQ_SUB), pusher(context, ZMQ_PUSH)
{
    srand(time(0));
    qCInfo(ecommercelog) << "eCommerce server starting...";

    // Initialize products
    products[1] = {"Apple iPhone 13", 799.00};
    products[2] = {"Samsung Galaxy S21", 699.00};
    products[3] = {"Sony WH-1000XM4 Headphones", 349.00};
    products[4] = {"Apple MacBook Pro 14\"", 1999.00};
    products[5] = {"Dell XPS 13 Laptop", 999.00};
    products[6] = {"Nintendo Switch", 299.00};
    products[7] = {"Amazon Echo Dot (4th Gen)", 49.99};
    products[8] = {"Fitbit Charge 5", 179.95};
    products[9] = {"Instant Pot Duo 7-in-1", 89.00};
    products[10] = {"Sony PlayStation 5", 499.00};

    try
    {
        subscriber.connect("tcp://benternet.pxl-ea-ict.be:24042");
        qCInfo(ecommercelog) << "Subscriber connected to endpoint.";
        pusher.connect("tcp://benternet.pxl-ea-ict.be:24041");
        qCInfo(ecommercelog) << "Pusher connected to endpoint.";

        const char* topic = "eCommerce?";
        subscriber.set(zmq::sockopt::subscribe, topic);
        subscriber.set(zmq::sockopt::rcvtimeo, 1000);

        zmq::message_t msg;
        qCInfo(ecommercelog) << "Subscribed to topic:" << topic;

        while (true)
        {
            try
            {
                if (subscriber.recv(msg, zmq::recv_flags::dontwait))
                {
                    std::string receivedMsg((char*)msg.data(), msg.size());
                    qCInfo(ecommercelog) << "Subscriber received:" << receivedMsg.c_str();

                    // Only handle incoming messages, not its own responses
                    if (receivedMsg.find("eCommerce!>") == std::string::npos) {
                        handleMessage(receivedMsg);
                    }
                }
                else
                {
                    // Timeout occurred, send a heartbeat
                    sendHeartbeat();
                    std::this_thread::sleep_for(std::chrono::seconds(5)); // Sleep to prevent busy waiting
                }
            }
            catch (zmq::error_t& ex)
            {
                qCCritical(ecommercelog) << "Caught an exception:" << ex.what();
                // Reconnect in case of an error
                subscriber.disconnect("tcp://benternet.pxl-ea-ict.be:24042");
                subscriber.connect("tcp://benternet.pxl-ea-ict.be:24042");
            }
        }
    }
    catch (zmq::error_t& ex)
    {
        qCCritical(ecommercelog) << "Caught an exception:" << ex.what();
    }
}

void eCommerce::sendHeartbeat()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::string heartbeat = "eCommerce?>keepalive>heartbeat>" + std::to_string(now_c);
    pusher.send(zmq::buffer(heartbeat), zmq::send_flags::none);
    qCInfo(ecommercelog) << "Sent heartbeat message with timestamp:" << now_c;
}

void eCommerce::receiveHeartbeat()
{
    std::string heartbeat = "eCommerce!>heartbeat>pulse";
    pusher.send(zmq::buffer(heartbeat), zmq::send_flags::none);
    qCInfo(ecommercelog) << "Received heartbeat message.";
}

void eCommerce::handleMessage(const std::string& msg)
{
    qCInfo(ecommercelog) << "Handling message:" << msg.c_str();
    if (msg.find("eCommerce?>") != std::string::npos)
    {
        // Split the message into segments
        std::stringstream ss(msg);
        std::string segment;
        std::vector<std::string> segments;

        while (std::getline(ss, segment, '>'))
        {
            segments.push_back(segment);
        }

        if (segments.size() < 3) {
            qCWarning(ecommercelog) << "Invalid message format:" << msg.c_str();
            return;
        }

        std::string username = segments[1];
        qCInfo(ecommercelog) << "Extracted username:" << username.c_str();

        if (segments[2] == "start")
        {
            qCInfo(ecommercelog) << "Detected start command.";
            std::string responseTopic = "eCommerce!>" + username + ">start";
            std::string welcomeMessage = getWelcomeMessage();
            std::string helpMessage = getHelpMessage();

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(welcomeMessage + "\n" + helpMessage), zmq::send_flags::none);

            qCInfo(ecommercelog) << "Sent welcome and help message to topic:" << responseTopic.c_str();
        }
        else if (segments[2] == "help")
        {
            qCInfo(ecommercelog) << "Detected help command.";
            std::string responseTopic = "eCommerce!>" + username + ">help";
            std::string helpMessage = getHelpMessage();

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(helpMessage), zmq::send_flags::none);

            qCInfo(ecommercelog) << "Sent help message to topic:" << responseTopic.c_str();
        }
        else if (segments[2] == "browseProducts")
        {
            qCInfo(ecommercelog) << "Detected browseProducts command.";
            std::string responseTopic = "eCommerce!>" + username + ">browseProducts";
            std::string productsMessage = getBrowseProductsMessage();

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(productsMessage), zmq::send_flags::none);

            qCInfo(ecommercelog) << "Sent browse products message to topic:" << responseTopic.c_str();
        }
        else if (segments[2] == "addToCart" && segments.size() == 5)
        {
            qCInfo(ecommercelog) << "Detected addToCart command.";

            try
            {
                int productId = std::stoi(segments[3]);
                int quantity = std::stoi(segments[4]);
                if (quantity <= 0) {
                    throw std::invalid_argument("Quantity must be greater than zero.");
                }
                qCInfo(ecommercelog) << "Parsed productId:" << productId << ", quantity:" << quantity;

                // Check if product ID exists
                if (products.find(productId) != products.end()) {
                    addToCart(username, productId, quantity);

                    std::string responseTopic = "eCommerce!>" + username + ">addToCart";
                    std::string cartMessage = "Added product " + std::to_string(productId) + " to cart with quantity " + std::to_string(quantity);

                    pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                    pusher.send(zmq::buffer(cartMessage), zmq::send_flags::none);

                    qCInfo(ecommercelog) << "Sent add to cart confirmation to topic:" << responseTopic.c_str();
                } else {
                    // Product ID does not exist
                    qCWarning(ecommercelog) << "Product ID " << productId << " does not exist.";
                    std::string responseTopic = "eCommerce!>" + username + ">addToCart";
                    std::string errorMessage = "Error: Product ID " + std::to_string(productId) + " does not exist.";

                    pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                    pusher.send(zmq::buffer(errorMessage), zmq::send_flags::none);

                    qCInfo(ecommercelog) << "Sent error message to topic:" << responseTopic.c_str();
                }
            }
            catch (const std::invalid_argument& e)
            {
                qCWarning(ecommercelog) << "Invalid argument in addToCart command:" << msg.c_str();
                std::string responseTopic = "eCommerce!>" + username + ">addToCart";
                std::string errorMessage = "Error: " + std::string(e.what());

                pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                pusher.send(zmq::buffer(errorMessage), zmq::send_flags::none);

                qCInfo(ecommercelog) << "Sent error message to topic:" << responseTopic.c_str();
            }
            catch (const std::out_of_range& e)
            {
                qCWarning(ecommercelog) << "Out of range error in addToCart command:" << msg.c_str();
                std::string responseTopic = "eCommerce!>" + username + ">addToCart";
                std::string errorMessage = "Error: " + std::string(e.what());

                pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                pusher.send(zmq::buffer(errorMessage), zmq::send_flags::none);

                qCInfo(ecommercelog) << "Sent error message to topic:" << responseTopic.c_str();
            }
        }
        else if (segments[2] == "removeFromCart" && segments.size() == 4)
        {
            qCInfo(ecommercelog) << "Detected removeFromCart command.";

            try
            {
                int productId = std::stoi(segments[3]);
                qCInfo(ecommercelog) << "Parsed productId:" << productId;
                if (canRemoveFromCart(username, productId)) {
                    removeFromCart(username, productId);

                    std::string responseTopic = "eCommerce!>" + username + ">removeFromCart";
                    std::string cartMessage = "Removed product " + std::to_string(productId) + " from cart.";

                    pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                    pusher.send(zmq::buffer(cartMessage), zmq::send_flags::none);

                    qCInfo(ecommercelog) << "Sent remove from cart confirmation to topic:" << responseTopic.c_str();
                } else {
                    std::string responseTopic = "eCommerce!>" + username + ">removeFromCart";
                    std::string errorMessage = "Error: Product ID " + std::to_string(productId) + " cannot be removed from cart as it is not in the cart.";

                    pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                    pusher.send(zmq::buffer(errorMessage), zmq::send_flags::none);

                    qCWarning(ecommercelog) << "Product ID " << productId << " cannot be removed from cart as it is not in the cart.";
                }
            }
            catch (const std::invalid_argument& e)
            {
                qCWarning(ecommercelog) << "Invalid argument in removeFromCart command:" << msg.c_str();
                std::string responseTopic = "eCommerce!>" + username + ">removeFromCart";
                std::string errorMessage = "Error: " + std::string(e.what());

                pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                pusher.send(zmq::buffer(errorMessage), zmq::send_flags::none);

                qCInfo(ecommercelog) << "Sent error message to topic:" << responseTopic.c_str();
            }
            catch (const std::out_of_range& e)
            {
                qCWarning(ecommercelog) << "Out of range error in removeFromCart command:" << msg.c_str();
                std::string responseTopic = "eCommerce!>" + username + ">removeFromCart";
                std::string errorMessage = "Error: " + std::string(e.what());

                pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                pusher.send(zmq::buffer(errorMessage), zmq::send_flags::none);

                qCInfo(ecommercelog) << "Sent error message to topic:" << responseTopic.c_str();
            }
        }
        else if (segments[2] == "viewCart")
        {
            qCInfo(ecommercelog) << "Detected viewCart command.";
            std::string responseTopic = "eCommerce!>" + username + ">viewCart";
            std::string cartMessage = viewCart(username);

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(cartMessage), zmq::send_flags::none);

            qCInfo(ecommercelog) << "Sent view cart message to topic:" << responseTopic.c_str();
        }
        else if (segments[2] == "checkout")
        {
            qCInfo(ecommercelog) << "Detected checkout command.";
            checkout(username);
        }
        else if (segments[2] == "pay")
        {
            qCInfo(ecommercelog) << "Detected pay command.";
            pay(username);
        }
        else if (segments[2] == "viewOrders")
        {
            qCInfo(ecommercelog) << "Detected viewOrders command.";
            std::string responseTopic = "eCommerce!>" + username + ">viewOrders";
            std::string ordersMessage = viewOrders(username);

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(ordersMessage), zmq::send_flags::none);

            qCInfo(ecommercelog) << "Sent view orders message to topic:" << responseTopic.c_str();
        }
        else if (segments[2] == "stop")
        {
            qCInfo(ecommercelog) << "Detected stop command.";
            stop(username);
        }
        else if (segments[2] == "heartbeat")
        {
            qCInfo(ecommercelog) << "Received heartbeat message.";
            receiveHeartbeat();
        }
        else
        {
            qCWarning(ecommercelog) << "Unknown command:" << msg.c_str();
        }
    }
}

std::string eCommerce::getHelpMessage()
{
    std::string info = "Available commands:\n"
                       "1. browseProducts - Display a list of available products.\n"
                       "2. searchProducts <keyword> - Search for products by keyword.\n"
                       "3. addToCart <productId> <quantity> - Add a product to the shopping cart.\n"
                       "4. removeFromCart <productId> - Remove a product from the shopping cart.\n"
                       "5. viewCart - View the contents of the shopping cart.\n"
                       "6. checkout - Process the checkout and place an order.\n"
                       "7. pay - Complete the payment for your order.\n"
                       "8. viewOrders - View past orders.\n"
                       "9. stop - Log out and clear the cart.\n";
    qCInfo(ecommercelog) << "Generated help message.";
    return info;
}

std::string eCommerce::getWelcomeMessage()
{
    std::string welcome = "Welcome to the eCommerce system!\n";
    welcome += "We are delighted to have you on board.\n";
    welcome += "This system allows you to browse products, add them to your cart, and make purchases with ease.\n";
    welcome += "To get started, you can use the following commands:\n";
    welcome += "1. browseProducts - Display a list of available products.\n";
    welcome += "2. searchProducts <keyword> - Search for products by keyword.\n";
    welcome += "3. addToCart <productId> <quantity> - Add a product to the shopping cart.\n";
    welcome += "4. removeFromCart <productId> - Remove a product from the shopping cart.\n";
    welcome += "5. viewCart - View the contents of the shopping cart.\n";
    welcome += "6. checkout - Process the checkout and place an order.\n";
    welcome += "7. pay - Complete the payment for your order.\n";
    welcome += "8. viewOrders - View past orders.\n";
    welcome += "9. stop - Log out and clear the cart.\n";
    welcome += "If you need any assistance, please use the 'help' command or contact our support team.\n";
    welcome += "Happy shopping!";
    qCInfo(ecommercelog) << "Generated welcome message.";
    return welcome;
}

std::string eCommerce::getBrowseProductsMessage()
{
    std::string products = "Available products:\n";
    for (const auto& product : this->products) {
        products += std::to_string(product.first) + ". " + product.second.first + " - $" + std::to_string(product.second.second) + "\n";
    }
    qCInfo(ecommercelog) << "Generated browse products message.";
    return products;
}

void eCommerce::addToCart(const std::string& username, int productId, int quantity)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (this->products.find(productId) != this->products.end()) {
        this->userCarts[username][productId] += quantity;
        qCInfo(ecommercelog) << "Added product" << productId << "to cart for user" << username.c_str() << "with quantity" << quantity;
    } else {
        qCWarning(ecommercelog) << "Product" << productId << "does not exist.";
    }
}

bool eCommerce::canRemoveFromCart(const std::string& username, int productId)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (this->userCarts.find(username) != this->userCarts.end() && this->userCarts[username].find(productId) != this->userCarts[username].end()) {
        return true;
    } else {
        qCWarning(ecommercelog) << "Product" << productId << "is not in the cart for user" << username.c_str();
        return false;
    }
}

void eCommerce::removeFromCart(const std::string& username, int productId)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (this->userCarts.find(username) != this->userCarts.end() && this->userCarts[username].find(productId) != this->userCarts[username].end()) {
        this->userCarts[username].erase(productId);
        qCInfo(ecommercelog) << "Removed product" << productId << "from cart for user" << username.c_str();
    } else {
        qCWarning(ecommercelog) << "Product" << productId << "is not in the cart for user" << username.c_str();
    }
}

std::string eCommerce::viewCart(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    std::string cartMessage = "Cart contents for " + username + ":\n";
    double total = 0.0;
    if (this->userCarts.find(username) != this->userCarts.end()) {
        for (const auto& item : this->userCarts[username]) {
            cartMessage += this->products[item.first].first + " - Quantity: " + std::to_string(item.second) + " - $" + std::to_string(this->products[item.first].second * item.second) + "\n";
            total += this->products[item.first].second * item.second;
        }
    }
    cartMessage += "Total: $" + std::to_string(total) + "\n";
    qCInfo(ecommercelog) << "Generated view cart message for user" << username.c_str();
    return cartMessage;
}

void eCommerce::checkout(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (this->userCarts.find(username) != this->userCarts.end() && !this->userCarts[username].empty()) {
        this->orders[username].push_back(this->userCarts[username]);
        this->userCarts.erase(username);
        this->userPaymentStatus[username] = false; // Set payment status to false
        qCInfo(ecommercelog) << "Checked out cart for user:" << username.c_str();

        std::string responseTopic = "eCommerce!>" + username + ">checkout";
        std::string checkoutMessage = "Your order has been placed successfully. Please proceed to payment.";

        pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
        pusher.send(zmq::buffer(checkoutMessage), zmq::send_flags::none);

        qCInfo(ecommercelog) << "Sent checkout confirmation to topic:" << responseTopic.c_str();
    } else {
        std::string responseTopic = "eCommerce!>" + username + ">checkout";
        std::string checkoutMessage = "Your cart is empty. Cannot place an order.";

        pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
        pusher.send(zmq::buffer(checkoutMessage), zmq::send_flags::none);

        qCInfo(ecommercelog) << "Sent empty cart message to topic:" << responseTopic.c_str();
    }
}

void eCommerce::pay(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (this->orders.find(username) != this->orders.end() && !this->orders[username].empty())
    {
        this->userPaymentStatus[username] = true; // Set payment status to true
        qCInfo(ecommercelog) << "Payment received for user:" << username.c_str();

        std::string responseTopic = "eCommerce!>" + username + ">pay";
        std::string payMessage = "Your payment has been received. Thank you for your purchase!";

        pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
        pusher.send(zmq::buffer(payMessage), zmq::send_flags::none);

        qCInfo(ecommercelog) << "Sent payment confirmation to topic:" << responseTopic.c_str();
    }
    else
    {
        std::string responseTopic = "eCommerce!>" + username + ">pay";
        std::string payMessage = "No pending orders to pay for.";

        pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
        pusher.send(zmq::buffer(payMessage), zmq::send_flags::none);

        qCInfo(ecommercelog) << "Sent no pending orders message to topic:" << responseTopic.c_str();
    }
}

std::string eCommerce::viewOrders(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    std::string ordersMessage = "Past orders for " + username + ":\n";
    if (this->orders.find(username) != this->orders.end()) {
        int orderNumber = 1;
        for (const auto& order : this->orders[username]) {
            ordersMessage += "Order " + std::to_string(orderNumber++) + ":\n";
            double total = 0.0;
            for (const auto& item : order) {
                ordersMessage += this->products[item.first].first + " - Quantity: " + std::to_string(item.second) + " - $" + std::to_string(this->products[item.first].second * item.second) + "\n";
                total += this->products[item.first].second * item.second;
            }
            ordersMessage += "Total: $" + std::to_string(total) + "\n";
            ordersMessage += "Payment Status: " + std::string(this->userPaymentStatus[username] ? "Paid" : "Pending") + "\n";
        }
    } else {
        ordersMessage += "No orders found.\n";
    }
    qCInfo(ecommercelog) << "Generated view orders message for user" << username.c_str();
    return ordersMessage;
}

void eCommerce::stop(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    qCInfo(ecommercelog) << "Logging out user:" << username.c_str();
    this->userCarts.erase(username);
    this->userPaymentStatus.erase(username);
    qCInfo(ecommercelog) << "Cleared cart and payment status for user:" << username.c_str();

    std::string responseTopic = "eCommerce!>" + username + ">stop";
    std::string logoutMessage = "User " + username + " has been logged out and their cart has been cleared.";

    pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
    pusher.send(zmq::buffer(logoutMessage), zmq::send_flags::none);

    qCInfo(ecommercelog) << "Sent logout confirmation to topic:" << responseTopic.c_str();
}
