#include "mainwindow.h"

#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>

#include <fstream>
#include <sstream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      infoLabel(new QLabel(this)),
      updateTimer(new QTimer(this))
{
    setWindowTitle("hwckr");
    resize(900, 550);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    infoLabel->setText("Loading hwckr system info...");
    infoLabel->setStyleSheet("font-family: monospace; font-size: 14px;");
    infoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    infoLabel->setWordWrap(true);

    layout->addWidget(infoLabel);
    setCentralWidget(central);

    updateSystemInfo();

    connect(updateTimer, &QTimer::timeout, this, [this]() {
        updateSystemInfo();
    });

    updateTimer->start(1000);
}

void MainWindow::updateSystemInfo()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::stringstream buffer;
    buffer << cpuinfo.rdbuf();

    QString text = QString::fromStdString(buffer.str());

    if (text.size() > 3500) {
        text = text.left(3500) + "\n\n...truncated by hwckr...";
    }

    infoLabel->setText(text);
}
