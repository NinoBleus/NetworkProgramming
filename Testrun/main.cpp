#include <iostream>
#include <zmq.hpp>
#include <thread>
#include <vector>
#include <random>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(n)    Sleep(n)
#endif

void sendCommand(zmq::socket_t& socket, const std::string& command)
{
    socket.send(zmq::buffer(command), zmq::send_flags::none);
    std::cout << "Pushed: " << command << std::endl;
    sleep(1); // Ensure there's a delay between commands
}

void runTestScenario(zmq::context_t& context, int userId)
{
    zmq::socket_t socket(context, ZMQ_PUSH);
    socket.connect("tcp://benternet.pxl-ea-ict.be:24041");

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> productDist(1, 10);
    std::uniform_int_distribution<> quantityDist(1, 4);
    std::uniform_int_distribution<> addToCartCountDist(1, 10);
    std::bernoulli_distribution payDist(0.5);

    int addToCartCount = addToCartCountDist(gen);

    // Initial set of commands
    std::vector<std::string> commands = {
        "eCommerce>User" + std::to_string(userId) + ">start?",
        "eCommerce>User" + std::to_string(userId) + ">help?",
        "eCommerce>User" + std::to_string(userId) + ">browseProducts?"
    };

    // Add random addToCart commands
    for (int i = 0; i < addToCartCount; ++i)
    {
        commands.push_back("eCommerce>User" + std::to_string(userId) + ">addToCart?>" + std::to_string(productDist(gen)) + ">" + std::to_string(quantityDist(gen)));
    }

    // Remaining set of commands
    commands.push_back("eCommerce>User" + std::to_string(userId) + ">viewCart?");
    commands.push_back("eCommerce>User" + std::to_string(userId) + ">checkout?");

    // 50/50 chance to add the pay command
    if (payDist(gen)) {
        commands.push_back("eCommerce>User" + std::to_string(userId) + ">pay?");
    }

    commands.push_back("eCommerce>User" + std::to_string(userId) + ">viewOrders?");
    commands.push_back("eCommerce>User" + std::to_string(userId) + ">stop?");

    for (const auto& command : commands)
    {
        sendCommand(socket, command);
    }
}

int main(void)
{
    try
    {
        zmq::context_t context(1);
        std::vector<std::thread> threads;

        // Create and run test scenarios for 5 users
        for (int i = 1; i <= 5; ++i)
        {
            threads.emplace_back(runTestScenario, std::ref(context), i);
        }

        // Join all threads
        for (auto& thread : threads)
        {
            thread.join();
        }
    }
    catch (zmq::error_t & ex)
    {
        std::cerr << "Caught an exception: " << ex.what() << std::endl;
    }

    return 0;
}
