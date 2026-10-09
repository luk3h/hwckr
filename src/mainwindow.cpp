#include "mainwindow.h"
#include "sensorstab.h"
#include "summarytab.h"
#include "systeminfo.h"
#include "theme.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent),
	  pages(new QStackedWidget(this)),
	  summaryTab(new SummaryTab()),
	  sensorsTab(new SensorsTab()),
	  pauseButton(nullptr),
	  logButton(nullptr),
	  uptimeLabel(new QLabel(this)),
	  sensorCountLabel(new QLabel(this)),
	  logLabel(new QLabel(this)),
	  updateTimer(new QTimer(this))
{
	setWindowTitle("hwckr");
	setWindowIcon(QIcon(":/assets/hwckr.png"));
	resize(1200, 780);
	setMinimumSize(900, 600);

	//--------------------------
	// Layout: header bar on top, pages below
	//--------------------------
	auto *central = new QWidget(this);
	auto *layout = new QVBoxLayout(central);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(buildHeader());

	pages->addWidget(summaryTab);
	pages->addWidget(sensorsTab);
	layout->addWidget(pages, 1);
	setCentralWidget(central);

	//--------------------------
	// Status bar
	//--------------------------
	auto *host = new QLabel(QString::fromStdString(SystemInfo::hostname() + "  ·  " +
	                                                SystemInfo::osName() + "  ·  Linux " +
	                                                SystemInfo::kernel()), this);
	statusBar()->addWidget(host);
	statusBar()->addPermanentWidget(logLabel);
	statusBar()->addPermanentWidget(sensorCountLabel);
	statusBar()->addPermanentWidget(uptimeLabel);
	statusBar()->setSizeGripEnabled(false);

	//--------------------------
	// Refresh timer
	//--------------------------
	connect(updateTimer, &QTimer::timeout, this, &MainWindow::tick);
	tick();                                          // first reading (rates need two)
	QTimer::singleShot(250, this, &MainWindow::tick); // fill rates in quickly
	updateTimer->start(1000);
}

MainWindow::~MainWindow()
{
	sensorsTab->stopLog();
}

QWidget *MainWindow::buildHeader()
{
	auto *bar = new QFrame(this);
	bar->setObjectName("HeaderBar");
	bar->setFixedHeight(58);
	auto *h = new QHBoxLayout(bar);
	h->setContentsMargins(16, 0, 16, 0);
	h->setSpacing(10);

	// Logo + name
	auto *logo = new QLabel(bar);
	QIcon appIcon(":/assets/hwckr.png");
	logo->setPixmap(appIcon.pixmap(30, 30));
	h->addWidget(logo);

	auto *nameBox = new QVBoxLayout();
	nameBox->setSpacing(0);
	auto *name = new QLabel("hw<span style='color:#36d1dc'>ckr</span>", bar);
	name->setObjectName("AppTitle");
	name->setTextFormat(Qt::RichText);
	auto *tagline = new QLabel("hardware information & monitoring", bar);
	tagline->setObjectName("AppSubtitle");
	nameBox->addStretch();
	nameBox->addWidget(name);
	nameBox->addWidget(tagline);
	nameBox->addStretch();
	h->addLayout(nameBox);
	h->addSpacing(28);

	// Page navigation
	auto *nav = new QButtonGroup(bar);
	nav->setExclusive(true);
	const QStringList names = {"Summary", "Sensors"};
	for (int i = 0; i < names.size(); i++) {
		auto *b = new QToolButton(bar);
		b->setObjectName("NavButton");
		b->setText(names[i]);
		b->setCheckable(true);
		b->setChecked(i == 0);
		b->setCursor(Qt::PointingHandCursor);
		b->setFixedHeight(58);
		nav->addButton(b, i);
		h->addWidget(b);
	}
	connect(nav, &QButtonGroup::idClicked, pages, &QStackedWidget::setCurrentIndex);

	h->addStretch();

	// Refresh rate: segmented buttons  [0.5s|1s|2s|5s]
	auto *refreshLabel = new QLabel("REFRESH", bar);
	refreshLabel->setObjectName("CardTitle");
	h->addWidget(refreshLabel);

	auto *segment = new QFrame(bar);
	segment->setObjectName("Segment");
	auto *seg = new QHBoxLayout(segment);
	segment->setFixedHeight(32);
	seg->setContentsMargins(3, 3, 3, 3);
	seg->setSpacing(2);
	auto *rates = new QButtonGroup(segment);
	rates->setExclusive(true);
	const QList<QPair<QString, int>> options = {{"0.5s", 500}, {"1s", 1000}, {"2s", 2000}, {"5s", 5000}};
	for (const auto &[text, ms] : options) {
		auto *b = new QToolButton(segment);
		b->setObjectName("SegmentButton");
		b->setFixedHeight(24);
		b->setText(text);
		b->setCheckable(true);
		b->setChecked(ms == 1000);
		b->setCursor(Qt::PointingHandCursor);
		rates->addButton(b, ms);
		seg->addWidget(b);
	}
	connect(rates, &QButtonGroup::idClicked, this, [this](int ms) {
		intervalMs = ms;
		if (updateTimer->isActive())
			updateTimer->start(ms);
	});
	h->addWidget(segment);
	h->addSpacing(6);

	pauseButton = new QPushButton("Pause", bar);
	pauseButton->setCheckable(true);
	pauseButton->setCursor(Qt::PointingHandCursor);
	pauseButton->setFixedHeight(32);
	pauseButton->setToolTip("Freeze all readings");
	connect(pauseButton, &QPushButton::toggled, this, &MainWindow::togglePause);
	h->addWidget(pauseButton);

	auto *reset = new QPushButton("Reset Min/Max", bar);
	reset->setCursor(Qt::PointingHandCursor);
	reset->setFixedHeight(32);
	reset->setToolTip("Clear minimum, maximum, average and graphs");
	connect(reset, &QPushButton::clicked, sensorsTab, &SensorsTab::resetStats);
	h->addWidget(reset);

	logButton = new QPushButton("● Log to CSV", bar);
	logButton->setObjectName("Danger");
	logButton->setCheckable(true);
	logButton->setCursor(Qt::PointingHandCursor);
	logButton->setFixedHeight(32);
	logButton->setToolTip("Record every sensor to a spreadsheet file");
	connect(logButton, &QPushButton::toggled, this, &MainWindow::toggleLog);
	h->addWidget(logButton);

	return bar;
}

void MainWindow::tick()
{
	std::vector<Reading> readings = sensorsTab->refresh();
	summaryTab->updateLive(readings);

	uptimeLabel->setText("Uptime " + QString::fromStdString(SystemInfo::uptime()));
	sensorCountLabel->setText(QString::number(sensorsTab->sensorCount()) + " sensors");
}

void MainWindow::togglePause(bool paused)
{
	if (paused) {
		updateTimer->stop();
		pauseButton->setText("Resume");
	} else {
		updateTimer->start(intervalMs);
		pauseButton->setText("Pause");
	}
}

void MainWindow::toggleLog(bool on)
{
	if (!on) {
		sensorsTab->stopLog();
		logButton->setText("● Log to CSV");
		logLabel->clear();
		return;
	}

	QString suggested = QDir::homePath() + "/hwckr-" +
	                    QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".csv";
	QString path = QFileDialog::getSaveFileName(this, "Log sensors to CSV", suggested, "CSV files (*.csv)");

	if (path.isEmpty() || !sensorsTab->startLog(path)) {
		if (!path.isEmpty())
			QMessageBox::warning(this, "hwckr", "Couldn't open " + path + " for writing.");
		logButton->blockSignals(true);
		logButton->setChecked(false);
		logButton->blockSignals(false);
		return;
	}
	logButton->setText("■ Stop Logging");
	logLabel->setText("<span style='color:#ff4d5e'>● REC</span>  " + QFileInfo(path).fileName());
}
