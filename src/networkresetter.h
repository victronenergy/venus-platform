#ifndef NETWORKRESETTER_H
#define NETWORKRESETTER_H

#include <QObject>
#include <list>
#include <QProcess>
#include <veutil/qt/ve_qitem.hpp>

class NetworkResetter : public QObject
{
	Q_OBJECT

	VeQItemSettings *mSettings = nullptr;

	std::list<QProcess> mStopProcesses;

	void stopAllServices();
	void makeLEDsIndicateReset();
	void setSettingsToDefault();
	void deletePaths();

private slots:
	void onStopperFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void onConnmanStarted(int exitCode, QProcess::ExitStatus exitStatus);
public:
	explicit NetworkResetter(VeQItemSettings *settings, QObject *parent = nullptr);

	void resetNetwork();

signals:

};

#endif // NETWORKRESETTER_H
