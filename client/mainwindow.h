#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <zmq.hpp>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void browseProductsButton_clicked();
    void addToCartButton_clicked();
    void clearCartButton_clicked();
    void viewCartButton_clicked();
    void checkoutButton_clicked();
    void payButton_clicked();
    void viewOrdersButton_clicked();
    void checkForIncomingMessages();

private:
    Ui::MainWindow *ui;
    zmq::context_t context;
    zmq::socket_t pusher;
    zmq::socket_t subscriber;
    QTimer *messageTimer;

    void sendMessage(const std::string& message);
    std::string receiveMessage();
    std::string getUsername();
};
#endif // MAINWINDOW_H
