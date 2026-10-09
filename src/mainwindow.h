#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class SensorsTab;
class SummaryTab;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

private:
	QWidget *buildHeader();
	void tick();
	void togglePause(bool paused);
	void toggleLog(bool on);

	QStackedWidget *pages;
	SummaryTab *summaryTab;
	SensorsTab *sensorsTab;

	QPushButton *pauseButton;
	QPushButton *logButton;
	int intervalMs = 1000;

	QLabel *uptimeLabel;
	QLabel *sensorCountLabel;
	QLabel *logLabel;

	QTimer *updateTimer;
};
