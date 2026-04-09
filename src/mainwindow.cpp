#include "mainwindow.h"

#include <QIcon>
#include <QLabel>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>
#include <Qt>

#include <fstream>
#include <sstream>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent),
	  tabs(new QTabWidget(this)),
	  overviewTab(new QWidget()),
	  cpuTab(new QWidget()),
	  overviewLabel(new QLabel(this)),
	  cpuLabel(new QLabel(this)),
	  updateTimer(new QTimer(this)),
	  memoryTab(new QWidget()),
	  memoryLabel(new QLabel(this))

{
	setWindowTitle("hwckr");
	setWindowIcon(QIcon("/home/lukeh/Documents/hwckr/src/assets/hwckr.png"));
	resize(900, 500);

	setupTabs();
	setCentralWidget(tabs);

	updateSystemInfo();

	connect(updateTimer, &QTimer::timeout, this, [this]() {
		updateSystemInfo();
	});

	updateTimer->start(1000);
}

void MainWindow::setupTabs()
{
	//--------------------------
	// Overview Tab
	//--------------------------
	auto *overviewLayout = new QVBoxLayout(overviewTab);

	overviewLabel->setText("Loading overview...");
	overviewLabel->setStyleSheet("font-family: monospace; font-size: 14px;");
	overviewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	overviewLabel->setWordWrap(true);

	overviewLayout->addWidget(overviewLabel);

	
	//--------------------------
	// CPU Tab
	//--------------------------
	auto *cpuLayout = new QVBoxLayout(cpuTab);

	cpuLabel->setText("Loading CPU info...");
	cpuLabel->setStyleSheet("font-family: monospace; font-size: 14px;");
	cpuLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	cpuLabel->setWordWrap(true);

	cpuLayout->addWidget(cpuLabel);


	
	//--------------------------
	// Memory Tab
	//--------------------------

	auto *memoryLayout = new QVBoxLayout(memoryTab);

	memoryLabel->setText("Loading Memory info...");
	memoryLabel->setStyleSheet("font-family: monospace; font-size: 14px;");
	memoryLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	memoryLabel->setWordWrap(true);

	memoryLayout->addWidget(memoryLabel);


	//--------------------------
	// Add tabs to tab widget
	//-------------------------
	tabs->addTab(overviewTab, "Overview");
	tabs->addTab(cpuTab, "CPU");
	tabs->addTab(memoryTab, "Memory");
}

void MainWindow::updateSystemInfo()
{
	std::ifstream cpuinfo("/proc/cpuinfo");
	std::stringstream buffer;
	buffer << cpuinfo.rdbuf();

	std::string cpuText = buffer.str();
	

	//--------------------------
	// Overview tab content
	//--------------------------
	QString overviewText = "hwckr Overview\n\n";
	overviewText += "System status: Running\n";
	overviewText += "Data source: /proc/cpuinfo\n";
	overviewText += "CPU info loaded successfully.\n";

	overviewLabel->setText(overviewText);

	//--------------------------
	// CPU tab content
	//--------------------------
	QString fullCpuText = QString::fromStdString(cpuText);

	if (fullCpuText.size() > 3500) {
		fullCpuText = fullCpuText.left(3500) + "\n\n...truncated by hwckr...";
	}

	cpuLabel->setText(fullCpuText);

	
	//--------------------------
	// Memory tab content
	//--------------------------
	QString memoryText = "Memory tab goes here";

	memoryLabel->setText(memoryText);
}


