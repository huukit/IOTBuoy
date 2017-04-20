#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <stdint.h>
#include <QFileDialog>
#include <QFile>

namespace Ui {
class Widget;
}

typedef struct _measStruct{
  uint32_t battmV;
  float measuredvbat;
  float airTemp;
  float airPressureHpa;
  float airHumidity;
  uint32_t sensorCount;
  float tempArray[5];
}measStruct;

class Widget : public QWidget
{
    Q_OBJECT

public:
    static const uint32_t preableLength = 5;
    explicit Widget(QWidget *parent = 0);
    ~Widget();

private slots:
    void on_butConnect_clicked();

    void handleReadyRead();
    void handleTimeout();
    void handleError(QSerialPort::SerialPortError error);

    void on_butSelectFile_clicked();

    void on_checkLog_stateChanged(int arg1);

private:
    void parse();

    Ui::Widget *ui;
    QSerialPort sport;
    QByteArray dataBuffer;
    uint8_t bytesExpected;

    bool log;
    QString filename;
    QFile logFile;
};

#endif // WIDGET_H
