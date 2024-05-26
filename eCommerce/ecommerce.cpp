#include "ecommerce.h"
#include <iostream>
#include <QCoreApplication>
#include <cstdlib>
#include <ctime>
#include <windows.h>
#include <zmq.hpp>
#include <mutex>
#include <stdexcept>
#include <sstream>

eCommerce::eCommerce(QCoreApplication *a) : context(1), subscriber(context, ZMQ_SUB), pusher(context, ZMQ_PUSH)
{
    srand(time(0));
    std::cout << "eCommerce server starting..." << std::endl;

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
        pusher.connect("tcp://benternet.pxl-ea-ict.be:24041");
        std::cout << "Connected to subscriber and pusher endpoints." << std::endl;

        const char* topic = "eCommerce";
        subscriber.setsockopt(ZMQ_SUBSCRIBE, topic, strlen(topic));
        zmq::message_t* msg = new zmq::message_t();
        std::cout << "Subscribed to topic: " << topic << std::endl;

        while (subscriber.connected())
        {
            subscriber.recv(msg);
            std::string receivedMsg((char*)msg->data(), msg->size());
            std::cout << "Subscriber received: [" << receivedMsg << "]" << std::endl;

            handleMessage(receivedMsg);
        }
    }
    catch (zmq::error_t& ex)
    {
        std::cerr << "Caught an exception: " << ex.what() << std::endl;
    }
}

void eCommerce::handleMessage(const std::string& msg)
{
    std::cout << "Handling message: " << msg << std::endl;
    if (msg.find("eCommerce>") != std::string::npos)
    {
        size_t start = msg.find("eCommerce>") + 10;
        size_t end = msg.find(">", start);
        std::string username = msg.substr(start, end - start);
        std::cout << "Extracted username: " << username << std::endl;

        if (msg.find(">start?") != std::string::npos)
        {
            std::cout << "Detected start command." << std::endl;
            std::string responseTopic = "eCommerce>" + username + ">start!";
            std::string welcomeMessage = getWelcomeMessage();
            std::string helpMessage = getHelpMessage();

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(welcomeMessage + "\n" + helpMessage), zmq::send_flags::none);

            std::cout << "Sent welcome and help message to topic: " << responseTopic << std::endl;
        }
        else if (msg.find(">help?") != std::string::npos)
        {
            std::cout << "Detected help command." << std::endl;
            std::string responseTopic = "eCommerce>" + username + ">help!";
            std::string helpMessage = getHelpMessage();

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(helpMessage), zmq::send_flags::none);

            std::cout << "Sent help message to topic: " << responseTopic << std::endl;
        }
        else if (msg.find(">browseProducts?") != std::string::npos)
        {
            std::cout << "Detected browseProducts command." << std::endl;
            std::string responseTopic = "eCommerce>" + username + ">browseProducts!";
            std::string productsMessage = getBrowseProductsMessage();

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(productsMessage), zmq::send_flags::none);

            std::cout << "Sent browse products message to topic: " << responseTopic << std::endl;
        }
        else if (msg.find(">addToCart?") != std::string::npos)
        {
            std::cout << "Detected addToCart command." << std::endl;
            std::stringstream ss(msg);
            std::string segment;
            std::vector<std::string> segments;

            while (std::getline(ss, segment, '>'))
            {
                segments.push_back(segment);
            }

            if (segments.size() == 5)
            {
                try
                {
                    int productId = std::stoi(segments[3]);
                    int quantity = std::stoi(segments[4]);
                    std::cout << "Parsed productId: " << productId << ", quantity: " << quantity << std::endl; // Debugging output
                    addToCart(username, productId, quantity);

                    std::string responseTopic = "eCommerce>" + username + ">addToCart!";
                    std::string cartMessage = "Added product " + std::to_string(productId) + " to cart with quantity " + std::to_string(quantity);

                    pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
                    pusher.send(zmq::buffer(cartMessage), zmq::send_flags::none);

                    std::cout << "Sent add to cart confirmation to topic: " << responseTopic << std::endl;
                }
                catch (const std::invalid_argument& e)
                {
                    std::cerr << "Invalid argument in addToCart command: " << msg << std::endl;
                }
                catch (const std::out_of_range& e)
                {
                    std::cerr << "Out of range error in addToCart command: " << msg << std::endl;
                }
            }
            else
            {
                std::cerr << "Error parsing addToCart command: " << msg << std::endl;
            }
        }
        else if (msg.find(">viewCart?") != std::string::npos)
        {
            std::cout << "Detected viewCart command." << std::endl;
            std::string responseTopic = "eCommerce>" + username + ">viewCart!";
            std::string cartMessage = viewCart(username);

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(cartMessage), zmq::send_flags::none);

            std::cout << "Sent view cart message to topic: " << responseTopic << std::endl;
        }
        else if (msg.find(">checkout?") != std::string::npos)
        {
            std::cout << "Detected checkout command." << std::endl;
            checkout(username);
        }
        else if (msg.find(">pay?") != std::string::npos)
        {
            std::cout << "Detected pay command." << std::endl;
            pay(username);
        }
        else if (msg.find(">viewOrders?") != std::string::npos)
        {
            std::cout << "Detected viewOrders command." << std::endl;
            std::string responseTopic = "eCommerce>" + username + ">viewOrders!";
            std::string ordersMessage = viewOrders(username);

            pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
            pusher.send(zmq::buffer(ordersMessage), zmq::send_flags::none);

            std::cout << "Sent view orders message to topic: " << responseTopic << std::endl;
        }
        else if (msg.find(">stop?") != std::string::npos)
        {
            std::cout << "Detected stop command." << std::endl;
            stop(username);
        }
    }
}

std::string eCommerce::getHelpMessage()
{
    std::string info = "Available commands:\n"
                       "1. browseProducts - Display a list of available products.\n"
                       "2. addToCart <productId> <quantity> - Add a product to the shopping cart.\n"
                       "3. removeFromCart <productId> - Remove a product from the shopping cart.\n"
                       "4. viewCart - View the contents of the shopping cart.\n"
                       "5. checkout - Process the checkout and place an order.\n"
                       "6. pay - Complete the payment for your order.\n"
                       "7. viewOrders - View past orders.\n"
                       "8. stop - Log out and clear the cart.\n";
    std::cout << "Generated help message." << std::endl;
    return info;
}

std::string eCommerce::getWelcomeMessage()
{
    std::string welcome = "Welcome to the eCommerce system!\n";
    welcome += "We are delighted to have you on board.\n";
    welcome += "This system allows you to browse products, add them to your cart, and make purchases with ease.\n";
    welcome += "To get started, you can use the following commands:\n";
    welcome += "1. browseProducts - Display a list of available products.\n";
    welcome += "2. addToCart <productId> <quantity> - Add a product to the shopping cart.\n";
    welcome += "3. removeFromCart <productId> - Remove a product from the shopping cart.\n";
    welcome += "4. viewCart - View the contents of the shopping cart.\n";
    welcome += "5. checkout - Process the checkout and place an order.\n";
    welcome += "6. pay - Complete the payment for your order.\n";
    welcome += "7. viewOrders - View past orders.\n";
    welcome += "8. stop - Log out and clear the cart.\n";
    welcome += "If you need any assistance, please use the 'help' command or contact our support team.\n";
    welcome += "Happy shopping!";
    std::cout << "Generated welcome message." << std::endl;
    return welcome;
}

std::string eCommerce::getBrowseProductsMessage()
{
    std::string products = "Available products:\n";
    for (const auto& product : this->products) {
        products += std::to_string(product.first) + ". " + product.second.first + " - $" + std::to_string(product.second.second) + "\n";
    }
    std::cout << "Generated browse products message." << std::endl;
    return products;
}

void eCommerce::addToCart(const std::string& username, int productId, int quantity)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (products.find(productId) != products.end()) {
        userCarts[username][productId] += quantity;
        std::cout << "Added product " << productId << " to cart for user " << username << " with quantity " << quantity << "." << std::endl;
    } else {
        std::cout << "Product " << productId << " does not exist." << std::endl;
    }
}

std::string eCommerce::viewCart(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    std::string cartMessage = "Cart contents for " + username + ":\n";
    double total = 0.0;
    if (userCarts.find(username) != userCarts.end()) {
        for (const auto& item : userCarts[username]) {
            cartMessage += products[item.first].first + " - Quantity: " + std::to_string(item.second) + " - $" + std::to_string(products[item.first].second * item.second) + "\n";
            total += products[item.first].second * item.second;
        }
    }
    cartMessage += "Total: $" + std::to_string(total) + "\n";
    std::cout << "Generated view cart message for user " << username << "." << std::endl;
    return cartMessage;
}

void eCommerce::checkout(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (userCarts.find(username) != userCarts.end() && !userCarts[username].empty()) {
        orders[username].push_back(userCarts[username]);
        userCarts.erase(username);
        userPaymentStatus[username] = false; // Set payment status to false
        std::cout << "Checked out cart for user: " << username << std::endl;

        std::string responseTopic = "eCommerce>" + username + ">checkout!";
        std::string checkoutMessage = "Your order has been placed successfully. Please proceed to payment.";

        pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
        pusher.send(zmq::buffer(checkoutMessage), zmq::send_flags::none);

        std::cout << "Sent checkout confirmation to topic: " << responseTopic << std::endl;
    } else {
        std::string responseTopic = "eCommerce>" + username + ">checkout!";
        std::string checkoutMessage = "Your cart is empty. Cannot place an order.";

        pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
        pusher.send(zmq::buffer(checkoutMessage), zmq::send_flags::none);

        std::cout << "Sent empty cart message to topic: " << responseTopic << std::endl;
    }
}

void eCommerce::pay(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    if (orders.find(username) != orders.end() && !orders[username].empty())
    {
        userPaymentStatus[username] = true; // Set payment status to true
        std::cout << "Payment received for user: " << username << std::endl;

        std::string responseTopic = "eCommerce>" + username + ">pay!";
        std::string payMessage = "Your payment has been received. Thank you for your purchase!";

        pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
        pusher.send(zmq::buffer(payMessage), zmq::send_flags::none);

        std::cout << "Sent payment confirmation to topic: " << responseTopic << std::endl;
    }
    else
    {
        std::string responseTopic = "eCommerce>" + username + ">pay!";
        std::string payMessage = "No pending orders to pay for.";

        pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
        pusher.send(zmq::buffer(payMessage), zmq::send_flags::none);

        std::cout << "Sent no pending orders message to topic: " << responseTopic << std::endl;
    }
}

std::string eCommerce::viewOrders(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    std::string ordersMessage = "Past orders for " + username + ":\n";
    if (orders.find(username) != orders.end()) {
        int orderNumber = 1;
        for (const auto& order : orders[username]) {
            ordersMessage += "Order " + std::to_string(orderNumber++) + ":\n";
            double total = 0.0;
            for (const auto& item : order) {
                ordersMessage += products[item.first].first + " - Quantity: " + std::to_string(item.second) + " - $" + std::to_string(products[item.first].second * item.second) + "\n";
                total += products[item.first].second * item.second;
            }
            ordersMessage += "Total: $" + std::to_string(total) + "\n";
            ordersMessage += "Payment Status: " + std::string(userPaymentStatus[username] ? "Paid" : "Pending") + "\n";
        }
    } else {
        ordersMessage += "No orders found.\n";
    }
    std::cout << "Generated view orders message for user " << username << "." << std::endl;
    return ordersMessage;
}

void eCommerce::stop(const std::string& username)
{
    std::lock_guard<std::mutex> lock(cartMutex);
    std::cout << "Logging out user: " << username << std::endl;
    userCarts.erase(username);
    userPaymentStatus.erase(username);
    std::cout << "Cleared cart and payment status for user: " << username << std::endl;

    std::string responseTopic = "eCommerce>" + username + ">stop!";
    std::string logoutMessage = "User " + username + " has been logged out and their cart has been cleared.";

    pusher.send(zmq::buffer(responseTopic), zmq::send_flags::sndmore);
    pusher.send(zmq::buffer(logoutMessage), zmq::send_flags::none);

    std::cout << "Sent logout confirmation to topic: " << responseTopic << std::endl;
}
