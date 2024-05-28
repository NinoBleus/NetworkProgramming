#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , context(1)
    , pusher(context, ZMQ_PUSH)
    , subscriber(context, ZMQ_SUB)
    , messageTimer(new QTimer(this))
{
    ui->setupUi(this);

    pusher.connect("tcp://benternet.pxl-ea-ict.be:24041");
    subscriber.connect("tcp://benternet.pxl-ea-ict.be:24042");
    subscriber.set(zmq::sockopt::subscribe, "eCommerce!");

    // Setting up UI signals and slots
    connect(ui->browseProductsButton, &QPushButton::clicked, this, &MainWindow::browseProductsButton_clicked);
    connect(ui->addToCartButton, &QPushButton::clicked, this, &MainWindow::addToCartButton_clicked);
    connect(ui->removeFromCartButton, &QPushButton::clicked, this, &MainWindow::removeFromCartButton_clicked);
    connect(ui->viewCartButton, &QPushButton::clicked, this, &MainWindow::viewCartButton_clicked);
    connect(ui->checkoutButton, &QPushButton::clicked, this, &MainWindow::checkoutButton_clicked);
    connect(ui->payButton, &QPushButton::clicked, this, &MainWindow::payButton_clicked);
    connect(ui->viewOrdersButton, &QPushButton::clicked, this, &MainWindow::viewOrdersButton_clicked);

    // Setting up the timer to check for incoming messages
    connect(messageTimer, &QTimer::timeout, this, &MainWindow::checkForIncomingMessages);
    messageTimer->start(10);
}

MainWindow::~MainWindow()
{
    delete ui;
}

std::string MainWindow::getUsername()
{
    return ui->usernameLineEdit->text().toStdString();
}

void MainWindow::sendMessage(const std::string& message)
{
    qDebug() << "Sending message: " << QString::fromStdString(message);
    pusher.send(zmq::buffer(message), zmq::send_flags::none);
}

void MainWindow::browseProductsButton_clicked()
{
    qDebug() << "Browse Products Button Clicked";
    std::string username = getUsername();
    if (username.empty()) {
        QMessageBox::warning(this, "Error", "Username cannot be empty");
        return;
    }
    sendMessage("eCommerce?>" + username + ">browseProducts");
}

void MainWindow::addToCartButton_clicked()
{
    qDebug() << "Add to Cart Button Clicked";
    std::string username = getUsername();
    if (username.empty()) {
        QMessageBox::warning(this, "Error", "Username cannot be empty");
        return;
    }
    int productId = ui->productIdLineEdit->text().toInt();
    int quantity = ui->quantityLineEdit->text().toInt();
    std::string message = "eCommerce?>" + username + ">addToCart>" + std::to_string(productId) + ">" + std::to_string(quantity);
    sendMessage(message);
}

void MainWindow::removeFromCartButton_clicked()
{
    qDebug() << "Removed from Cart Button Clicked";
    std::string username = getUsername();
    if (username.empty()) {
        QMessageBox::warning(this, "Error", "Username cannot be empty");
        return;
    }
    int productId = ui->productIdLineEdit->text().toInt();
    int quantity = ui->quantityLineEdit->text().toInt();
    std::string message = "eCommerce?>" + username + ">removeFromCart>" + std::to_string(productId);
    sendMessage(message);
}

void MainWindow::viewCartButton_clicked()
{
    qDebug() << "View Cart Button Clicked";
    std::string username = getUsername();
    if (username.empty()) {
        QMessageBox::warning(this, "Error", "Username cannot be empty");
        return;
    }
    sendMessage("eCommerce?>" + username + ">viewCart");
}

void MainWindow::checkoutButton_clicked()
{
    qDebug() << "Checkout Button Clicked";
    std::string username = getUsername();
    if (username.empty()) {
        QMessageBox::warning(this, "Error", "Username cannot be empty");
        return;
    }
    sendMessage("eCommerce?>" + username + ">checkout");
}

void MainWindow::payButton_clicked()
{
    qDebug() << "Pay Button Clicked";
    std::string username = getUsername();
    if (username.empty()) {
        QMessageBox::warning(this, "Error", "Username cannot be empty");
        return;
    }
    sendMessage("eCommerce?>" + username + ">pay");
}

void MainWindow::viewOrdersButton_clicked()
{
    qDebug() << "View Orders Button Clicked";
    std::string username = getUsername();
    if (username.empty()) {
        QMessageBox::warning(this, "Error", "Username cannot be empty");
        return;
    }
    sendMessage("eCommerce?>" + username + ">viewOrders");
}

void MainWindow::checkForIncomingMessages()
{
    while (true) {
        zmq::message_t msg;
        auto result = subscriber.recv(msg, zmq::recv_flags::dontwait);
        if (!result) break;
        std::string receivedMsg(static_cast<char*>(msg.data()), msg.size());

        // Filter out heartbeat messages
        if (receivedMsg.find("heartbeat") != std::string::npos) {
            continue;
        }

        ui->messagesListWidget->addItem(QString::fromStdString(receivedMsg));
        ui->messagesListWidget->scrollToBottom();
    }
}
