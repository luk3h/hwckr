#pragma once

#include <QMainWindow>

class QTabWidget;
class QWidget;
class QLabel;
class QTimer;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

private:
	QTabWidget *tabs;
	QWidget *overviewTab;
	QWidget *cpuTab;
	QWidget *memoryTab;

	QLabel *overviewLabel;
	QLabel *cpuLabel;
	QLabel *memoryLabel;
	QTimer *updateTimer;

	void setupTabs();
	void updateSystemInfo();

	QString getCpuModel();
	int getCoreCount();
	QString getTotalRam();
	QString getHostname();
	QString getUptime();

};
