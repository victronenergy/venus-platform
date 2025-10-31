#pragma once

#include <QObject>
#include <QTimer>

#include <veutil/qt/ve_qitem.hpp>

// This uses a lot of blocking calls, because this is called in an application
// that only does this.
class NetworkReset : public QObject
{
	Q_OBJECT

	VeQItemSettings *mSettings = nullptr;
	int mDefaultValueSetCounter = 0;
	QTimer mSetDefaultTimeoutTimer;

	void initiate();
	void stopRelevantServices();
	void makeLedsIndicateReset();
	void setSettingsToDefault();
	void deletePaths();

private slots:
	void initiateSettingsShutdownAndReboot();
	void defaultValueSetStateChanged(VeQItem::State state);

public:
	explicit NetworkReset(VeQItemSettings *settings, QObject *parent = nullptr);

	void reset();
};
