#pragma once

#include "sensorreader.h"
#include "systeminfo.h"

#include <QWidget>

#include <vector>

class QGridLayout;
class QLabel;
class QProgressBar;
class QTreeWidget;
class QWidget;

// HWiNFO-style system summary: live stat cards along the top,
// device tree on the left, details for the selected device on the right.
class SummaryTab : public QWidget {
	Q_OBJECT

public:
	explicit SummaryTab(QWidget *parent = nullptr);

	// Update the live cards from the latest sensor readings.
	void updateLive(const std::vector<Reading> &readings);

private:
	struct StatCard {
		QLabel *value = nullptr;
		QLabel *sub = nullptr;
		QProgressBar *bar = nullptr;
	};

	StatCard makeCard(const QString &title, Category category, QWidget *parent, QGridLayout *grid, int column);
	void showDevice(int index);

	std::vector<Device> devices;

	StatCard cpuLoad, cpuClock, cpuTemp, memLoad;

	QTreeWidget *deviceTree;
	QLabel *deviceIcon;
	QLabel *deviceKind;
	QLabel *deviceTitle;
	QWidget *featureBox;
	QGridLayout *featureGrid;
	QTreeWidget *details;
};
