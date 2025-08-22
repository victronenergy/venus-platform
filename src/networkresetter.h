#ifndef NETWORKRESETTER_H
#define NETWORKRESETTER_H

#include <QObject>
#include <list>
#include <QProcess>
#include <QTimer>
#include <chrono>
#include <veutil/qt/ve_qitem.hpp>


/**
 * This uses a lot of blocking calls, because this is called in an application that only does this.
 */
class NetworkResetter : public QObject
{
	Q_OBJECT

	VeQItemSettings *mSettings = nullptr;
	int mDefaultValueSetCounter = 0;
	QTimer mSetDefaultTimeoutTimer;

	void initiate();
	void stopRelevantServices();
	void makeLEDsIndicateReset();
	void setSettingsToDefault();
	void deletePaths();

private slots:
	void initiateSettingsShutdownAndReboot();
	void defaultValueSetStateChanged(VeQItem::State state);

public:
	explicit NetworkResetter(VeQItemSettings *settings, QObject *parent = nullptr);

	void resetNetwork();

signals:

};

#endif // NETWORKRESETTER_H
