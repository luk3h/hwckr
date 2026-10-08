#include "sensorstab.h"
#include "theme.h"

#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

enum Column { ColSensor, ColCurrent, ColMin, ColMax, ColAvg, ColGraph, ColCount };

// Extra data stored on each row for the custom painters.
constexpr int HistoryRole = Qt::UserRole + 1;  // QVariantList of doubles
constexpr int ColourRole = Qt::UserRole + 2;   // QColor of the device category
constexpr int PercentRole = Qt::UserRole + 3;  // 0-100 for % sensors, drives the load bar
constexpr int CategoryRole = Qt::UserRole + 4; // sort order of device rows

constexpr size_t HistoryLength = 40; // samples kept for the graph

int decimalsFor(const std::string &unit)
{
	if (unit == "V") return 3;
	if (unit == "A" || unit == "GB") return 2;
	if (unit == "RPM" || unit == "MHz" || unit == "MB") return 0;
	return 1; // °C, W, %, MB/s, KB/s
}

QString format(double v, const std::string &unit)
{
	return QString::number(v, 'f', decimalsFor(unit)) + " " + QString::fromStdString(unit);
}

// Draws a thin load bar along the bottom of "Current" cells for % sensors.
class ValueDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override
	{
		QStyledItemDelegate::paint(p, opt, idx);

		QVariant pct = idx.data(PercentRole);
		if (!pct.isValid())
			return;
		double v = std::clamp(pct.toDouble(), 0.0, 100.0);

		QRectF track(opt.rect.left() + 8, opt.rect.bottom() - 3, opt.rect.width() - 16, 2);
		p->save();
		p->setRenderHint(QPainter::Antialiasing);
		p->setPen(Qt::NoPen);
		p->setBrush(Theme::Border);
		p->drawRoundedRect(track, 1, 1);
		QColor c = v >= 90 ? Theme::Hot : v >= 70 ? Theme::Warm : idx.data(ColourRole).value<QColor>();
		p->setBrush(c);
		p->drawRoundedRect(QRectF(track.left(), track.top(), track.width() * v / 100.0, 2), 1, 1);
		p->restore();
	}
};

// Draws a live sparkline of the sensor's recent history.
class GraphDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override
	{
		QStyledItemDelegate::paint(p, opt, idx); // background / selection

		QVariantList hist = idx.data(HistoryRole).toList();
		if (hist.size() < 2)
			return;

		double lo = hist[0].toDouble(), hi = lo;
		for (const QVariant &v : hist) {
			lo = std::min(lo, v.toDouble());
			hi = std::max(hi, v.toDouble());
		}
		// Percentages use a fixed 0-100 scale; everything else auto-scales.
		if (idx.data(PercentRole).isValid()) {
			lo = 0;
			hi = 100;
		} else if (hi - lo < 1e-9) {
			lo -= 1;
			hi += 1;
		} else {
			double pad = (hi - lo) * 0.15;
			lo -= pad;
			hi += pad;
		}

		QRectF r = QRectF(opt.rect).adjusted(6, 4, -6, -4);
		double step = r.width() / (HistoryLength - 1);
		double x0 = r.right() - step * (hist.size() - 1); // newest sample on the right

		QPainterPath line;
		for (int i = 0; i < hist.size(); i++) {
			double y = r.bottom() - (hist[i].toDouble() - lo) / (hi - lo) * r.height();
			QPointF pt(x0 + i * step, y);
			if (i == 0)
				line.moveTo(pt);
			else
				line.lineTo(pt);
		}

		QColor c = idx.data(ColourRole).value<QColor>();
		QPainterPath area = line;
		area.lineTo(r.right(), r.bottom());
		area.lineTo(x0, r.bottom());
		area.closeSubpath();

		QLinearGradient grad(r.topLeft(), r.bottomLeft());
		QColor top = c, bottom = c;
		top.setAlpha(90);
		bottom.setAlpha(0);
		grad.setColorAt(0, top);
		grad.setColorAt(1, bottom);

		p->save();
		p->setRenderHint(QPainter::Antialiasing);
		p->setPen(Qt::NoPen);
		p->fillPath(area, grad);
		p->setPen(QPen(c, 1.4));
		p->drawPath(line);
		p->restore();
	}
};

} // namespace

SensorsTab::SensorsTab(QWidget *parent)
	: QWidget(parent),
	  tree(new QTreeWidget(this)),
	  filter(new QLineEdit(this))
{
	setObjectName("Page");
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(14, 12, 14, 12);
	layout->setSpacing(10);

	filter->setPlaceholderText("Filter sensors…  (e.g. temp, fan, core 3)");
	filter->setClearButtonEnabled(true);
	connect(filter, &QLineEdit::textChanged, this, &SensorsTab::applyFilter);
	layout->addWidget(filter);

	tree->setColumnCount(ColCount);
	tree->setHeaderLabels({"Sensor", "Current", "Minimum", "Maximum", "Average", "History"});
	tree->setAlternatingRowColors(true);
	tree->setRootIsDecorated(true);
	tree->setUniformRowHeights(false);
	tree->setIndentation(16);
	tree->setIconSize(QSize(18, 18));
	tree->setSelectionMode(QAbstractItemView::SingleSelection);
	tree->setItemDelegate(new Theme::SectionDelegate(tree));
	tree->setItemDelegateForColumn(ColCurrent, new ValueDelegate(tree));
	tree->setItemDelegateForColumn(ColGraph, new GraphDelegate(tree));

	auto *header = tree->header();
	header->setStretchLastSection(false);
	header->setSectionResizeMode(ColSensor, QHeaderView::Stretch);
	for (int c = ColCurrent; c <= ColAvg; c++) {
		header->setSectionResizeMode(c, QHeaderView::Interactive);
		tree->setColumnWidth(c, 115);
		tree->headerItem()->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
	}
	header->setSectionResizeMode(ColGraph, QHeaderView::Interactive);
	tree->setColumnWidth(ColGraph, 150);

	layout->addWidget(tree);
}

std::vector<Reading> SensorsTab::refresh()
{
	static const QFont mono = [] {
		QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		f.setPointSizeF(f.pointSizeF() * 0.95);
		return f;
	}();

	std::vector<Reading> readings = reader.read();

	for (const Reading &r : readings) {
		QColor catColour = Theme::categoryColour(r.category);

		// Device row (created once)
		QTreeWidgetItem *&group = groups[r.group];
		if (!group) {
			// Keep devices in category order (CPU, GPU, memory, board, drives, network),
			// even when some only appear after the first reading.
			int rank = static_cast<int>(r.category);
			int pos = tree->topLevelItemCount();
			for (int i = 0; i < tree->topLevelItemCount(); i++) {
				if (tree->topLevelItem(i)->data(ColSensor, CategoryRole).toInt() > rank) {
					pos = i;
					break;
				}
			}
			group = new QTreeWidgetItem();
			tree->insertTopLevelItem(pos, group);
			group->setText(ColSensor, QString::fromStdString(r.group));
			group->setIcon(ColSensor, Theme::categoryIcon(r.category));
			group->setData(ColSensor, CategoryRole, rank);
			group->setData(ColSensor, Theme::SectionRole, true);
			group->setData(ColSensor, Theme::AccentRole, catColour);
			group->setSizeHint(ColSensor, QSize(0, 32));
			group->setFirstColumnSpanned(true);
			group->setExpanded(true);
		}

		// Sensor row (created once)
		const std::string id = r.group + "|" + r.key;
		Row &row = rows[id];
		if (!row.item) {
			row.item = new QTreeWidgetItem(group);
			row.item->setText(ColSensor, QString::fromStdString(r.label));
			row.item->setSizeHint(ColSensor, QSize(0, 24));
			row.item->setData(ColGraph, ColourRole, catColour);
			row.item->setData(ColCurrent, ColourRole, catColour);
			for (int c = ColCurrent; c <= ColAvg; c++) {
				row.item->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
				row.item->setFont(c, mono);
			}
			row.item->setForeground(ColMin, Theme::Muted);
			row.item->setForeground(ColAvg, Theme::Muted);
			row.unit = r.unit;
			order.push_back(id);
			if (!filter->text().isEmpty())
				applyFilter();
		}

		// Running stats
		if (row.count == 0) {
			row.min = row.max = r.value;
		} else {
			row.min = std::min(row.min, r.value);
			row.max = std::max(row.max, r.value);
		}
		row.current = r.value;
		row.sum += r.value;
		row.count++;
		row.history.push_back(r.value);
		while (row.history.size() > HistoryLength)
			row.history.pop_front();

		QTreeWidgetItem *it = row.item;
		it->setText(ColCurrent, format(row.current, r.unit));
		it->setText(ColMin, format(row.min, r.unit));
		it->setText(ColMax, format(row.max, r.unit));
		it->setText(ColAvg, format(row.sum / row.count, r.unit));
		it->setForeground(ColCurrent, Theme::valueColour(row.current, r.unit));
		QColor maxColour = Theme::valueColour(row.max, r.unit);
		it->setForeground(ColMax, maxColour == Theme::Text ? Theme::Muted : maxColour);

		if (r.unit == "%")
			it->setData(ColCurrent, PercentRole, row.current);
		if (r.unit == "%")
			it->setData(ColGraph, PercentRole, true);

		QVariantList hist;
		for (double v : row.history)
			hist.append(v);
		it->setData(ColGraph, HistoryRole, hist);
	}

	// CSV logging
	if (log.is_open()) {
		log << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
		for (const std::string &id : logColumns) {
			auto found = rows.find(id);
			log << ",";
			if (found != rows.end())
				log << found->second.current;
		}
		log << "\n";
		log.flush();
	}

	return readings;
}

void SensorsTab::resetStats()
{
	for (auto &[id, row] : rows) {
		row.min = row.max = row.current;
		row.sum = row.current;
		row.count = 1;
		row.history.clear();
		row.history.push_back(row.current);
		row.item->setText(ColMin, format(row.min, row.unit));
		row.item->setText(ColMax, format(row.max, row.unit));
		row.item->setText(ColAvg, format(row.current, row.unit));
	}
}

bool SensorsTab::startLog(const QString &path)
{
	log.open(path.toStdString(), std::ios::out | std::ios::trunc);
	if (!log.is_open())
		return false;

	// Columns are fixed to the sensors that exist when logging starts.
	logColumns = order;
	log << "Time";
	for (const std::string &id : logColumns) {
		const Row &row = rows[id];
		std::string group = id.substr(0, id.find('|'));
		std::string name = group + " - " + row.item->text(ColSensor).toStdString() + " [" + row.unit + "]";
		std::replace(name.begin(), name.end(), '"', '\'');
		log << ",\"" << name << "\"";
	}
	log << "\n";
	return true;
}

void SensorsTab::stopLog()
{
	if (log.is_open())
		log.close();
}

void SensorsTab::applyFilter()
{
	const QString text = filter->text().trimmed();

	for (auto &[name, group] : groups) {
		bool groupMatches = text.isEmpty() || group->text(ColSensor).contains(text, Qt::CaseInsensitive);
		bool anyVisible = false;
		for (int i = 0; i < group->childCount(); i++) {
			QTreeWidgetItem *child = group->child(i);
			bool show = groupMatches || child->text(ColSensor).contains(text, Qt::CaseInsensitive);
			child->setHidden(!show);
			anyVisible |= show;
		}
		group->setHidden(!anyVisible);
	}
}
