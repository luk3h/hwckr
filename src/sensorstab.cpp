#include "sensorstab.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>

namespace {

enum Column { ColSensor, ColCurrent, ColMin, ColMax, ColAvg, ColCount };

int decimalsFor(const std::string &unit)
{
	if (unit == "V") return 3;
	if (unit == "A" || unit == "GB") return 2;
	if (unit == "RPM" || unit == "MHz") return 0;
	return 1; // °C, W, %
}

QString format(double v, const std::string &unit)
{
	return QString::number(v, 'f', decimalsFor(unit)) + " " + QString::fromStdString(unit);
}

// Orange when warm, red when hot. Only applies to temperatures.
void applyColour(QTreeWidgetItem *row, int column, double v, const std::string &unit)
{
	if (unit == "°C" && v >= 85.0)
		row->setForeground(column, QBrush(QColor(220, 50, 50)));
	else if (unit == "°C" && v >= 70.0)
		row->setForeground(column, QBrush(QColor(230, 140, 0)));
	else
		row->setData(column, Qt::ForegroundRole, QVariant()); // back to normal text colour
}

} // namespace

SensorsTab::SensorsTab(QWidget *parent)
	: QWidget(parent),
	  tree(new QTreeWidget(this))
{
	auto *layout = new QVBoxLayout(this);

	// Toolbar
	auto *toolbar = new QHBoxLayout();
	auto *resetButton = new QPushButton("Reset Min/Max", this);
	connect(resetButton, &QPushButton::clicked, this, &SensorsTab::resetStats);
	toolbar->addWidget(resetButton);
	toolbar->addStretch();
	layout->addLayout(toolbar);

	// Table
	tree->setColumnCount(ColCount);
	tree->setHeaderLabels({"Sensor", "Current", "Minimum", "Maximum", "Average"});
	tree->setAlternatingRowColors(true);
	tree->setRootIsDecorated(true);
	tree->setUniformRowHeights(true);
	tree->setSelectionMode(QAbstractItemView::SingleSelection);

	auto *header = tree->header();
	header->setSectionResizeMode(ColSensor, QHeaderView::Stretch);
	for (int c = ColCurrent; c < ColCount; c++) {
		header->setSectionResizeMode(c, QHeaderView::Fixed);
		tree->setColumnWidth(c, 110);
		tree->headerItem()->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
	}
	header->setStretchLastSection(false);

	layout->addWidget(tree);
}

void SensorsTab::refresh()
{
	const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	for (const Reading &r : reader.read()) {
		// Device row (created once)
		QTreeWidgetItem *&group = groupItems[r.group];
		if (!group) {
			group = new QTreeWidgetItem(tree);
			group->setText(ColSensor, QString::fromStdString(r.group));
			QFont bold = group->font(ColSensor);
			bold.setBold(true);
			group->setFont(ColSensor, bold);
			group->setFirstColumnSpanned(true);
			group->setExpanded(true);
		}

		// Sensor row (created once)
		const std::string id = r.group + "|" + r.key;
		QTreeWidgetItem *&row = rowItems[id];
		if (!row) {
			row = new QTreeWidgetItem(group);
			row->setText(ColSensor, QString::fromStdString(r.label));
			for (int c = ColCurrent; c < ColCount; c++) {
				row->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
				row->setFont(c, mono);
			}
		}

		// Update running stats
		Stats &s = stats[id];
		if (s.count == 0) {
			s.min = s.max = r.value;
		} else {
			s.min = std::min(s.min, r.value);
			s.max = std::max(s.max, r.value);
		}
		s.current = r.value;
		s.sum += r.value;
		s.count++;

		row->setText(ColCurrent, format(s.current, r.unit));
		row->setText(ColMin, format(s.min, r.unit));
		row->setText(ColMax, format(s.max, r.unit));
		row->setText(ColAvg, format(s.sum / s.count, r.unit));

		applyColour(row, ColCurrent, s.current, r.unit);
		applyColour(row, ColMax, s.max, r.unit);
	}
}

void SensorsTab::resetStats()
{
	stats.clear(); // next refresh starts min/max/avg again from the current value
	refresh();
}
