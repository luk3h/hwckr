#pragma once

#include <QMainWindow>

class QLabel;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private:
    QLabel *infoLabel;
    QTimer *updateTimer;

    void updateSystemInfo();
};
