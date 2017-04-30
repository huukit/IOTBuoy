#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include <QDate>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    QList<QSerialPortInfo>::iterator i;
    for(i = ports.begin(); i != ports.end(); i++)
        ui->comboPorts->addItem(i->portName());

    bytesExpected = 0;
    log = false;
    filename.clear();

    connectionTimer.setInterval(1000);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::handleReadyRead(){
    dataBuffer.append(sport.readAll());
    parse();
}


void Widget::parse(){
    while(dataBuffer.size() > 0){
        if(bytesExpected == 0){
            // Try finding start.
            while(dataBuffer.size() > 0){
                if(dataBuffer.at(0) != '$')
                    dataBuffer.remove(0, 1);
                else
                    break;
            }

            // Not enough data yet.
            if(dataBuffer.size() < (preableLength + 1))
                return;

            uint8_t preamblesFound = 0;
            for(int i = 0; i < dataBuffer.size(); i++){
                // Symbol found.
                if(dataBuffer.at(i) == '$')preamblesFound++;
                // All symbols found.
                if(preamblesFound == preableLength)break;
                // Only limited symbols found.
                if(dataBuffer.at(i) == '$' && dataBuffer.at(i + 1) != '$')break;
            }

            dataBuffer.remove(0, preamblesFound);

            if(preamblesFound == preableLength){
                bytesExpected = dataBuffer.at(0);
                dataBuffer.remove(0, 1);
            }
        }

        else{
            // Not enough bytes to parse.
            if(dataBuffer.size() < bytesExpected)
                return;

            // Parse the message.
            qDebug() << "Parsing main message.";

            int8_t rssi = dataBuffer.at(1);
            uint8_t sender = dataBuffer.at(0);

            dataBuffer.remove(0, 2);

            measStruct * mstr;
            mstr = reinterpret_cast<measStruct *>(dataBuffer.data());

            // Add sender to combobox.
            uint32_t items = ui->comboID->count();
            bool found = false;

            for(uint32_t i = 0; i < items; i++){
                if(ui->comboID->itemText(i) == QString::number(sender)){
                    found = true;
                    break;
                }
            }

            if(!found && sender != 0)ui->comboID->addItem(QString::number(sender));

            // Update UI if we have node selected.
            if(ui->comboID->currentText() == QString::number(sender)){
                if(mstr->dataVersion == dataStructVersion1){
                    ui->lineBatt->setText(QString::number(mstr->battmV));
                    ui->lineTemp->setText(QString::number(mstr->airTemp));
                    ui->linePress->setText(QString::number(mstr->airPressureHpa));
                    ui->lineRH->setText(QString::number(mstr->airHumidity));

                    ui->lineTempArrCnt->setText(QString::number(mstr->sensorCount));

                    ui->lineT1->setText(QString::number(mstr->tempArray[0]));
                    ui->lineT2->setText(QString::number(mstr->tempArray[1]));
                    ui->lineT3->setText(QString::number(mstr->tempArray[2]));
                    ui->lineT4->setText(QString::number(mstr->tempArray[3]));
                    ui->lineT5->setText(QString::number(mstr->tempArray[4]));

                    ui->lineLastUpdate->setText(QDateTime::currentDateTime().toString());

                    ui->lineID->setText(QString::number(sender));
                    ui->lineRSSI->setText(QString::number(rssi));
                }
                if(log){
                    QTextStream logstream(&logFile);
                    logstream << QDateTime::currentDateTime().toString(Qt::ISODate) <<  ",";
                    logstream << QString::number(mstr->battmV) <<  ",";

                    logstream << QString::number(mstr->airTemp) <<  ",";
                    logstream << QString::number(mstr->airPressureHpa) <<  ",";
                    logstream << QString::number(mstr->airHumidity) <<  ",";

                    for(int i = 0; i < mstr->sensorCount; i++){
                        logstream << QString::number(mstr->tempArray[i]) <<  ",";
                    }
                    logstream << endl;

                    // Let's generate a html page.
                    QFile htmlFile;
#if defined(Q_OS_WIN)
    htmlFile.setFileName("C:\\index.html");
#elif defined(Q_OS_LINUX)
    htmlFile.setFileName("/var/www/index.html");
#endif

                    if(!htmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate)){
                        qDebug() << "Cannot open html file.";
                    }
                    else{
                        QTextStream htmlStream(&htmlFile);
                        htmlStream << "<html><head><meta http-equiv=\"refresh\" content=\"30\"><title>IOTBuoy</title></head><body>" << endl;
                        htmlStream << "Buoy id: " << QString::number(sender) <<  "<br>" << endl;;
                        htmlStream << "RSSI: " << QString::number(rssi) << "dB" <<  "<br>" << endl;
                        htmlStream << "Date: " << QDateTime::currentDateTime().toString(Qt::ISODate) <<  "<br>" << endl;
                        htmlStream << "Battery voltage: " << QString::number(mstr->battmV) <<  "mV <br>" << endl;
                        htmlStream << "<b>Data: </b><br>" << endl;
                        htmlStream << "Air temp: " << QString::number(mstr->airTemp) <<  "C <br>" << endl;
                        htmlStream << "Air pressure: " << QString::number(mstr->airPressureHpa) <<  "hPa <br>" << endl;
                        htmlStream << "Air humidity: " << QString::number(mstr->airHumidity) <<  "\% <br>" << endl;
                        htmlStream << "Water temp array: <br>" << endl;

                        for(int i = 0; i < mstr->sensorCount; i++){
                            htmlStream << QString::number(i) << ": " << QString::number(mstr->tempArray[i]) <<  "C <br>" << endl;
                        }

                        htmlStream << "</body></html>" << endl;
                    }
                }

            }

            // Local stuff.
            if(sender == 0){
                if(mstr->dataVersion == deviceVersion){
                    ui->radServer->setChecked(true);
                }
            }

            dataBuffer.remove(0, bytesExpected - 2);
            bytesExpected = 0;
        }
    }

}

void Widget::on_checkLog_stateChanged(int arg1)
{
    if(ui->checkLog->isChecked()){
        if(filename.isEmpty()){
            on_butSelectFile_clicked();
        }
        logFile.setFileName(filename);
        if(!logFile.open(QIODevice::WriteOnly | QIODevice::Append)){
            qDebug() << "Cannot open log file.";
        }
        else{
            QTextStream logStream(&logFile);
            logStream << "date,battmV,temp,press,rh,water1,water2" << endl;
            log = true;
        }
    }
    else{
        log = false;
        logFile.close();
    }
}

void Widget::handleTimeout(){

}

void Widget::handleError(QSerialPort::SerialPortError error){

}

void Widget::on_butConnect_clicked()
{
    sport.setPortName(ui->comboPorts->currentText());
    sport.setBaudRate(QSerialPort::Baud115200);

    if (!sport.open(QIODevice::ReadWrite)) {
        qDebug() << QObject::tr("Failed to open port %1, error: %2").arg(ui->comboPorts->currentText()).arg(sport.errorString()) << endl;
    }

    connect(&sport, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
    connect(&connectionTimer, SIGNAL(timeout()), this, SLOT(sendVersionRequest()));
    connectionTimer.start();
    sendVersionRequest();

    ui->butConnect->setEnabled(false);
    ui->butDisconnect->setEnabled(true);
}

void Widget::sendVersionRequest(){
    ui->radServer->setChecked(false);
    sport.write("v\n");
}

void Widget::on_butSelectFile_clicked(){
    QFileDialog fdial;
    fdial.setViewMode(QFileDialog::Detail);
    filename = fdial.getSaveFileName(this, "Select logfile", "", "csv (*csv)");

    if(!filename.contains(".csv")){
       filename = filename.append(".csv");
    }
}

void Widget::on_butDisconnect_clicked(){
    sport.close();
    disconnect(&sport);
    connectionTimer.stop();
    disconnect(&connectionTimer);
    ui->butConnect->setEnabled(true);
    ui->butDisconnect->setEnabled(false);
}
