#pragma once

#include "sensorreader.h"

#include <QWidget>

#include <map>
#include <string>

class QTreeWidget;
class QTreeWidgetItem;

// HWiNFO-style sensors table: Sensor | Current | Minimum | Maximum | Average,
// grouped by device.
class SensorsTab : public QWidget {
	Q_OBJECT

public:
	explicit SensorsTab(QWidget *parent = nullptr);

	void refresh();      // take a new reading and update the table
	void resetStats();   // clear min/max/average

private:
	struct Stats {
		double current = 0, min = 0, max = 0, sum = 0;
		long count = 0;
	};

	SensorReader reader;
	QTreeWidget *tree;

	std::map<std::string, Stats> stats;                     // key: group + "|" + key
	std::map<std::string, QTreeWidgetItem *> groupItems;    // key: group
	std::map<std::string, QTreeWidgetItem *> rowItems;      // key: group + "|" + key
};
