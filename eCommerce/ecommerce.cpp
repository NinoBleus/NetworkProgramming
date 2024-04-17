#include "ecommerce.h"
#include <iostream>
#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QDateTime>
#include <cstdlib>
#include <ctime>
#include <windows.h>

eCommerce::eCommerce(QCoreApplication *a)
{
    srand(time(0));


    try
    {
        zmq::context_t context(1);


        zmq::socket_t pusher( context, ZMQ_PUSH );
        pusher.connect( "tcp://benternet.pxl-ea-ict.be:24041" );

        zmq::socket_t subscriber( context, ZMQ_SUB );
        subscriber.connect( "tcp://benternet.pxl-ea-ict.be:24042" );

        const char* topic = "eCommerce>help";
        subscriber.setsockopt(ZMQ_SUBSCRIBE, topic, strlen(topic));
        zmq::message_t * msg = new zmq::message_t();

        while( subscriber.connected() )
        {
            subscriber.recv( msg );
            std::cout << "Subscriber received : [" << std::string( (char*) msg->data(), msg->size() ) << "]" << std::endl;

            std::string receivedMsg((char*)msg->data(), msg->size());

            if (receivedMsg.find("eCommerce>help") != std::string::npos)
            {
                help();
            }
        }
    }
    catch( zmq::error_t & ex )
    {
        std::cerr << "Caught an exception : " << ex.what();
    }
}

void eCommerce::start(const std::vector<std::string>& messages)
{
    if( messages.empty() )
    {
        std::cout << "Received empty message !" << std::endl;
    }
    else
    {
        for (const std::string& msg : messages) {
            std::cout << std::endl;
            std::cout << "Received: " << msg << std::endl;
            if (msg.find("eCommerce>help") != std::string::npos)
            {
                help();
            }
            // if (msg.find("eCommerce>...") != std::string::npos)
            //     //template holder
            // }
            if(msg.find("eCommerce>exit") != std::string::npos)
            {
                pusher.send("Exiting now...", 1);
                stop();
            }
        }
    }
}

void eCommerce::help()
{
    zmq::socket_t pusher( context, ZMQ_PUSH );
    pusher.connect( "tcp://benternet.pxl-ea-ict.be:24041" );

    std::cout << "Available commands:\n";
    std::cout << "1. browseProducts - Display a list of available products.\n";
    std::cout << "2. searchProducts <keyword> - Search for products by keyword.\n";
    std::cout << "3. addToCart <productId> <quantity> - Add a product to the shopping cart.\n";
    std::cout << "4. removeFromCart <productId> - Remove a product from the shopping cart.\n";
    std::cout << "5. viewCart - View the contents of the shopping cart.\n";
    std::cout << "6. checkout - Process the checkout and place an order.\n";
    std::cout << "7. help - Display available commands.\n";

    const std::string info =  "Available commands:\n"
                    "1. browseProducts - Display a list of available products.\n"
                    "2. searchProducts <keyword> - Search for products by keyword.\n"
                    "3. addToCart <productId> <quantity> - Add a product to the shopping cart.\n"
                    "4. removeFromCart <productId> - Remove a product from the shopping cart.\n"
                    "5. viewCart - View the contents of the shopping cart.\n"
                    "6. checkout - Process the checkout and place an order.\n"
                    "7. help - Display available commands.\n";

    if(pusher.connected())
    {
        Sleep( 1000 );
        pusher.send(info.c_str(),info.size());
        std::cout << "Pushed : " << info << std::endl;
    }
    else
    {
        std::cout << "Pusher connection failed or not setup\n";
    }
}



void eCommerce::stop()
{
    std::cout << "  Exiting service... " <<std::endl;
    Sleep(2000);
    exit(0);
}
