/*
 * <copyright>
 * Copyright (c) 2017: Tuomas Huuki
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the (Lesser) GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * </copyright>
 */

/* $Id$ */

/*! \file
 * Really crude gui client for IOT weather buoy. (license: GPLv2 or LGPLv2.1)
 * \author Tuomas Huuki tuomas.huuki@proximia.fi
 * \copyright 2017 Tuomas Huuki / proximia.fi
 */

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
