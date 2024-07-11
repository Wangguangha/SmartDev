#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSqlQueryModel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initWindow();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initWindow()
{
    //初始化窗体
    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(serverNewConnect()));
    setNetwork();
    connectDatabase();
    tempThreshold = 10;          //温度阈值
    humiThreshold = 10;          //湿度阈值
    lightThreshold = 10;         //亮度阈值
    smokeThreshold = 10;         //烟雾浓度阈值

    Timer_= new QTimer();
    Timer_->setInterval(10000);	//6s
    connect(Timer_, SIGNAL(timeout()), this, SLOT(showDatabase()));
    Timer_->start();

    showDatabase();
}

void MainWindow::connectDatabase()
{
    // 变量声明
    db = QSqlDatabase::addDatabase("QSQLITE");

    db.setDatabaseName("SQLite/work.db");
    if(!db.open())
    {
        qDebug()<<"Error: Failed to connect database." << db.lastError();
    }else
    {
        qDebug() << "Succeed to connect database." ;
    }

    db.close();
}

void MainWindow::setNetwork()
{
    //开启网络服务
    int port = 8888;                //Tcp端口号为8888
    if(!tcpServer->listen(QHostAddress::Any, port))
    {
        qDebug() << "服务器端监听失败！！！";
        return;
    }else
    {
        qDebug() << "服务器端监听成功！！！";
    }
}

void MainWindow::serverNewConnect()
{
    //服务端新连接槽函数
    tcpSocket = tcpServer->nextPendingConnection();
    if(!tcpSocket)
    {
        qDebug() << "未能成功获取客户端连接！！！";
    }else
    {
        qDebug() << "成功建立连接！！！";
        connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(serverReadData()));
        connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(serverDisconnection()));
    }
}

void MainWindow::parseClientMsg(QString recvText)
{
    //传感器数据
    int startIndex = recvText.indexOf("TEMP:");
    int endIndex = recvText.indexOf("HUMI:");
    int temp, humi, light, smoke;

    if (startIndex != -1 && endIndex != -1) {
        QString tempStr = recvText.mid(startIndex + 5, endIndex - startIndex - 6);
        temp = tempStr.toInt();
        qDebug() << "Temperature:" << temp;
        ui->lcdNum_Temp->display(temp);
    }

    startIndex = recvText.indexOf("HUMI:");
    endIndex = recvText.indexOf("LIGHT:");

    if (startIndex != -1 && endIndex != -1) {
        QString humiStr = recvText.mid(startIndex + 5, endIndex - startIndex - 6);
        humi = humiStr.toInt();
        qDebug() << "Humidity:" << humi;
        ui->lcdNum_Hum->display(humi);
    }

    startIndex = recvText.indexOf("LIGHT:");
    endIndex = recvText.indexOf("SMOKE:");

    if (startIndex != -1 && endIndex != -1) {
        QString lightStr = recvText.mid(startIndex + 7, endIndex - startIndex - 8);
        light = lightStr.toInt();
        qDebug() << "Light:" << light;
        ui->lcdNum_Light->display(light);
    }

    startIndex = recvText.indexOf("SMOKE:");
    if (startIndex != -1) {
        QString smokeStr = recvText.mid(startIndex + 7);
        smoke = smokeStr.toInt();
        qDebug() << "Smoke:" << smoke;
        ui->lcdNum_Smoke->display(smoke);
    }

    checkSensorData(temp, humi, light, smoke);
}

void MainWindow::checkSensorData(int temp, int humi, int light, int smoke)
{
    this->readCount++;
    this->readCount2++;

    bool writeDataFlag = false;
    if((temp > tempThreshold) || (humi > tempThreshold))
    {
        //温度超过阈值，开启空调和风扇
        sendCtlToDev("AC_FAN_ON");
        this->tempHumiCount++;
        writeDataFlag = true;
    }

    if((temp < tempThreshold) && (humi < tempThreshold))
    {
        //温度低于阈值，关闭空调和风扇
        sendCtlToDev("AC_FAN_OFF");
    }

    if(light < lightThreshold)
    {
        //开灯
        sendCtlToDev("LED_ON");
    }

    if(light > lightThreshold)
    {
        //关灯
        sendCtlToDev("LED_OFF");
        this->lightCount++;
        writeDataFlag = true;
    }

    if(smoke > smokeThreshold)
    {
        //亮灯 & 蜂鸣器报警
        sendCtlToDev("LED_BUZZER_ON");
        this->somkeCount++;
        writeDataFlag = true;
    }

    if(smoke < smokeThreshold)
    {
        //亮灯 & 蜂鸣器取消报警
        sendCtlToDev("LED_BUZZER_OFF");
    }

    if(writeDataFlag)
    {
        writeToDatabase();
    }

    writeSensorToDatabase();
}

void MainWindow::writeSensorToDatabase()
{
    if(this->readCount < 10)
        return;
    else
        this->readCount = 0;

    //将传感器数据写入数据库
    if (!db.open()) {
         qDebug() << "Error: Unable to open database";
     }

     // 准备 SQL 查询
     QSqlQuery query;
     query.prepare("INSERT INTO sensorData "
                   "(temp, hum, smoke, light, tempMax, humMax, smokeMax, lightMax, dateTime) "
                   "VALUES (:temp, :hum, :smoke, :light, :tempMax, :humMax, :smokeMax, :lightMax, :dateTime)");

     QString dateStr = QDateTime::currentDateTime().toString("yyyy/MM/dd hh/mm/ss");


     QVariant tempValue = ui->lcdNum_Temp->value();
     QVariant humValue = ui->lcdNum_Hum->value();
     QVariant smokeValue = ui->lcdNum_Smoke->value();
     QVariant lightValue = ui->lcdNum_Light->value();
     QVariant tempMaxValue = ui->spinBox_temp_2->value();
     QVariant humMaxValue = ui->spinBox_humi_2->value();
     QVariant smokeMaxValue = ui->spinBox_somke_2->value();
     QVariant lightMaxValue = ui->spinBox_light_2->value();

     // 绑定数据
     query.bindValue(":temp", tempValue.toInt());
     query.bindValue(":hum", humValue.toInt());
     query.bindValue(":smoke", smokeValue.toInt());
     query.bindValue(":light", lightValue.toInt());
     query.bindValue(":tempMax", tempMaxValue.toInt());
     query.bindValue(":humMax", humMaxValue.toInt());
     query.bindValue(":smokeMax", smokeMaxValue.toInt());
     query.bindValue(":lightMax", lightMaxValue.toInt());
     query.bindValue(":dateTime", dateStr);


     // 执行查询
     if (!query.exec()) {
         qDebug() << "Error: Unable to execute query";
         qDebug() << query.lastError().text();
     }

     // 关闭数据库连接
     db.close();
}

void MainWindow::writeToDatabase()
{
    if(this->readCount2 > 10)
    {
        this->readCount2 = 0;
    }
    else
    {
        qDebug() << "The readCount is: " << this->readCount2;
        return;
    }

    //将数据写入数据库
    if (!db.open()) {
        qDebug() << "Error: Unable to open database";
    }

    QSqlQuery query;
    query.prepare("INSERT INTO warnInfo (tempHumCount, smokeCount, lightCount, date) "
                  "VALUES (:tempHumCount, :smokeCount, :lightCount, :date)");

    // 绑定数据
    query.bindValue(":tempHumCount", this->tempHumiCount/10);
    query.bindValue(":smokeCount", this->somkeCount/10);
    query.bindValue(":lightCount", this->lightCount/10);
    query.bindValue(":date", QDateTime::currentDateTime()); // 当前日期时间

    // 执行查询
    if (!query.exec()) {
        qDebug() << "Error: Unable to execute query";
        qDebug() << query.lastError().text();

    }

    // 关闭数据库连接
    db.close();
}

void MainWindow::sendSignalToBtn(int flag)
{
    switch (flag) {
        case State::LED_ON:
            LED_POWER = false;
            QMetaObject::invokeMethod(ui->pBtn_LED, "clicked", Qt::QueuedConnection);
            break;
        case State::LED_OFF:
            LED_POWER = true;
            QMetaObject::invokeMethod(ui->pBtn_LED, "clicked", Qt::QueuedConnection);
            break;
        case State::FAN_ON:
            FAN_POWER = false;
            QMetaObject::invokeMethod(ui->pBtn_FAN, "clicked", Qt::QueuedConnection);
            break;
        case State::FAN_OFF:
            FAN_POWER = true;
            QMetaObject::invokeMethod(ui->pBtn_FAN, "clicked", Qt::QueuedConnection);
            break;
        case State::AC_ON:
            AC_POWER = false;
            QMetaObject::invokeMethod(ui->pBtn_AC, "clicked", Qt::QueuedConnection);
            break;
        case State::AC_OFF:
            AC_POWER = true;
            QMetaObject::invokeMethod(ui->pBtn_AC, "clicked", Qt::QueuedConnection);
            break;
        case State::RING_ON:
            RING_POWER = false;
            QMetaObject::invokeMethod(ui->pBtn_RING, "clicked", Qt::QueuedConnection);
            break;
        case State::RING_OFF:
            RING_POWER = true;
            QMetaObject::invokeMethod(ui->pBtn_RING, "clicked", Qt::QueuedConnection);
            break;
        default:
            break;
    }
}

void MainWindow::serverReadData()
{
    //服务器接收数据函数
    int cmdFlag = 0;
    QString recvText = (QString)tcpSocket->readAll();
    ui->textEdit->append(recvText);

    parseClientMsg(recvText);
}

void MainWindow::serverDisconnection()
{
    //服务端断开连接
    qDebug() << "与客户端断开连接！！！";
    return;
}

void MainWindow::on_pBtn_AC_clicked()
{
    //空调开关按钮
    AC_POWER = !AC_POWER;
    if(AC_POWER){
        ui->pBtn_AC->setIcon(QIcon(":/image/POWER_ON.png"));
        ctlStr = "AC_ON";
    }else{
        ui->pBtn_AC->setIcon(QIcon(":/image/POWER_OFF.png"));
        ctlStr = "AC_OFF";
    }
    sendCtlToDev(ctlStr);
}

void MainWindow::on_pBtn_RING_clicked()
{
    //蜂鸣器开关
    RING_POWER = !RING_POWER;
    if(RING_POWER){
        ui->pBtn_RING->setIcon(QIcon(":/image/ring_ON.png"));
        ctlStr = "RING_ON";
    }else{
        ui->pBtn_RING->setIcon(QIcon(":/image/ring_OFF.png"));
        ctlStr = "RING_OFF";
    }
    sendCtlToDev(ctlStr);
}


void MainWindow::on_pBtn_FAN_clicked()
{
    //风扇开关按钮
    FAN_POWER = !FAN_POWER;
    if(FAN_POWER){
        ui->pBtn_FAN->setIcon(QIcon(":/image/FAN_ON.png"));
        ctlStr = "FAN_ON";
    }else{
        ui->pBtn_FAN->setIcon(QIcon(":/image/FAN_OFF.png"));
        ctlStr = "FAN_OFF";
    }
    sendCtlToDev(ctlStr);
}

void MainWindow::on_pBtn_LED_clicked()
{
    //LED开关按钮
    LED_POWER = !LED_POWER;
    if(LED_POWER){
        ui->pBtn_LED->setIcon(QIcon(":/image/LED_ON.png"));
        ctlStr = "LED_ON";
    }else{
        ui->pBtn_LED->setIcon(QIcon(":/image/LED_OFF.png"));
        ctlStr = "LED_OFF";
    }
    sendCtlToDev(ctlStr);
}

void MainWindow::sendCtlToDev(QString ctrStr)
{
    ctrStr.append(" ");
    tcpSocket->write(ctrStr.toLatin1());
    tcpSocket->flush();
}

void MainWindow::on_pBtn_commit_2_clicked()
{
    //提交告警阈值修改
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(nullptr, "提示", "你想要更新各个传感器数据阈值？",
                                 QMessageBox::Ok | QMessageBox::Cancel);

    if (reply == QMessageBox::Ok) {
       tempThreshold = ui->spinBox_temp_2->value();
       humiThreshold = ui->spinBox_humi_2->value();
       lightThreshold = ui->spinBox_light_2->value();
       smokeThreshold = ui->spinBox_somke_2->value();
    }
}

void MainWindow::showDatabase()
{
    if(!db.open())
    {
        qDebug()<<"Error: Failed to connect database." << db.lastError();
    }

    QSqlQueryModel *qryModel = new QSqlQueryModel(this);
    qryModel->setQuery("select * from sensorData;");
    qryModel->setHeaderData(0, Qt::Horizontal, "ID");
    qryModel->setHeaderData(1, Qt::Horizontal, "Temperature");
    qryModel->setHeaderData(2, Qt::Horizontal, "Humidity");
    qryModel->setHeaderData(3, Qt::Horizontal, "Smoke");
    qryModel->setHeaderData(4, Qt::Horizontal, "Light");
    qryModel->setHeaderData(5, Qt::Horizontal, "Max Temperature");
    qryModel->setHeaderData(6, Qt::Horizontal, "Max Humidity");
    qryModel->setHeaderData(7, Qt::Horizontal, "Max Smoke");
    qryModel->setHeaderData(8, Qt::Horizontal, "Max Light");
    qryModel->setHeaderData(9, Qt::Horizontal, "Date Time");

    QItemSelectionModel *theSelection=new QItemSelectionModel(qryModel);
    ui->tableView->setModel(qryModel);
    ui->tableView->setSelectionModel(theSelection);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);


    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery("select * from warnInfo;");
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Temperature and Humidity Count");
    model->setHeaderData(2, Qt::Horizontal, "Smoke Count");
    model->setHeaderData(3, Qt::Horizontal, "Light Count");
    model->setHeaderData(4, Qt::Horizontal, "Date");

    QItemSelectionModel *theSelection2=new QItemSelectionModel(model);
    ui->tableView_2->setModel(model);
    ui->tableView_2->setSelectionModel(theSelection2);
    ui->tableView_2->setSelectionBehavior(QAbstractItemView::SelectRows);

    db.close();
}
