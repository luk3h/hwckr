#pragma once

#include <QMainWindow>

class QComboBox;
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
	QComboBox *intervalBox;

	QLabel *uptimeLabel;
	QLabel *sensorCountLabel;
	QLabel *logLabel;

	QTimer *updateTimer;
};
