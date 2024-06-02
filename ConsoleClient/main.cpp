#include <iostream>
#include <string>
#include <zmq.hpp>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(n)    Sleep(n)
#endif

int main()
{
    try
    {
        // Create the ZMQ context with a single IO thread
        zmq::context_t context(1);

        // Initialize a push socket (ventilator)
        zmq::socket_t ventilator(context, ZMQ_PUSH);
        ventilator.connect("tcp://benternet.pxl-ea-ict.be:24041");

        // Initialize a subscriber socket
        zmq::socket_t subscriber(context, ZMQ_SUB);
        subscriber.connect("tcp://benternet.pxl-ea-ict.be:24042");  // Adjust the address as needed
        subscriber.setsockopt(ZMQ_SUBSCRIBE, "eCommerce!>", 11);    // Subscribe to the topic "eCommerce!>"

        std::string input;
        while (true)
        {
            std::cout << "Enter message to send: ";
            std::getline(std::cin, input);
            if (input.empty())
            {
                std::cout << "Empty input, exiting." << std::endl;
                break;
            }

            // Send message to the ventilator
            zmq::message_t message(input.size());
            memcpy(message.data(), input.data(), input.size());
            ventilator.send(message, zmq::send_flags::none);
            std::cout << "Pushed: [" << input << "]" << std::endl;

            // Poll to check for incoming messages with a timeout of 1 second
            zmq::pollitem_t items[] = { { static_cast<void*>(subscriber), 0, ZMQ_POLLIN, 0 } };
            zmq::poll(items, 1, 1000);  // Timeout set to 1000ms (1 second)

            if (items[0].revents & ZMQ_POLLIN)
            {
                // Check if there are any messages from the subscriber
                std::string received_str;
                zmq::message_t recv_message;
                while (true)
                {
                    subscriber.recv(recv_message, zmq::recv_flags::none);
                    received_str.append(static_cast<char*>(recv_message.data()), recv_message.size());

                    // Check if there are more parts
                    int more;
                    size_t more_size = sizeof(more);
                    subscriber.getsockopt(ZMQ_RCVMORE, &more, &more_size);
                    if (!more)
                        break;
                }
                std::cout << "Received on eCommerce!>: [" << received_str << "]" << std::endl;
            }
            else
            {
                std::cout << "No messages received within 1 second, continuing." << std::endl;
            }

            sleep(1);  // Sleep for 1 second
        }
    }
    catch (zmq::error_t& ex)
    {
        std::cerr << "Caught an exception: " << ex.what() << std::endl;
    }

    return 0;
}
