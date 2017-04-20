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

            if(log){
                QTextStream logstream(&logFile);
                logstream << QDate::currentDate().toString() <<  ",";
                logstream << QString::number(mstr->battmV) <<  ",";

                logstream << QString::number(mstr->airTemp) <<  ",";
                logstream << QString::number(mstr->airPressureHpa) <<  ",";
                logstream << QString::number(mstr->airHumidity) <<  ",";

                logstream << QString::number(mstr->tempArray[0]) <<  ",";
                logstream << QString::number(mstr->tempArray[1]) <<  endl;
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

    if (!sport.open(QIODevice::ReadOnly)) {
        qDebug() << QObject::tr("Failed to open port %1, error: %2").arg(ui->comboPorts->currentText()).arg(sport.errorString()) << endl;
    }

    connect(&sport, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
}

void Widget::on_butSelectFile_clicked()
{
    QFileDialog fdial;
    fdial.setViewMode(QFileDialog::Detail);
    filename = fdial.getSaveFileName(this, "Select logfile", "", "csv (*csv)");

    if(!filename.contains(".csv")){
       filename = filename.append(".csv");
    }
}


