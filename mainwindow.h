#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSqlDatabase>
#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QTimer>
#include <QDateTime>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum State {
           NONE,
           LED_ON,
           LED_OFF,
           FAN_ON,
           FAN_OFF,
           AC_ON,
           AC_OFF,
           RING_ON,
           RING_OFF
       };
       Q_ENUM(State)

    QTcpServer *tcpServer = new QTcpServer();   //Tcp服务端对象
    QTcpSocket *tcpSocket = new QTcpSocket();   //Tcp连接socket
    QSqlQuery sql_query;

    int tempHumiCount = 0;
    int somkeCount = 0;
    int lightCount = 0;

    int readCount = 0;
    int readCount2 = 0;

    void initWindow();                          //初始化窗体
    void setNetwork();                          //设置网络信息
    void parseClientMsg(QString recvText); //解析客户端发送的信息
    void sendSignalToBtn(int flag);             //发送相对应的处理信号
    void sendCtlToDev(QString ctrStr);          //发送控制消息到客户端
    void checkSensorData(int temp, int humi, int light, int smoke);   //检查传感器数据是否超过阈值
    void connectDatabase();                     //连接数据库

    void writeToDatabase();
    void writeSensorToDatabase();

    QTimer* Timer_;

private slots:
    void on_pBtn_AC_clicked();

    void on_pBtn_FAN_clicked();

    void on_pBtn_LED_clicked();

    void on_pBtn_RING_clicked();

    void serverNewConnect();                    //新的网络连接响应
    void serverReadData();                      //服务端读取数据
    void serverDisconnection();                 //服务端断开连接

    void showDatabase();
    void on_pBtn_commit_2_clicked();

private:
    Ui::MainWindow *ui;

    bool LED_POWER = false;     //LED开关
    bool FAN_POWER = false;     //风扇开关
    bool AC_POWER = false;      //空调开关
    bool RING_POWER = false;    //蜂鸣器开关

    int tempThreshold = 36;          //温度阈值
    int humiThreshold = 60;          //湿度阈值
    int lightThreshold = 1000;         //亮度阈值
    int smokeThreshold = 20;         //烟雾浓度阈值

    QString ctlStr;             //发送到客户端的控制命令
    QSqlDatabase db;
};
#endif // MAINWINDOW_H
