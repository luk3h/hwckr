#include "mainwindow.h"
#include "theme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("hwckr");
	Theme::apply(app);

	MainWindow window;
	window.show();

	return app.exec();
}
