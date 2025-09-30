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

	void initiate();
	void stopAllServices();
	void makeLEDsIndicateReset();
	void setSettingsToDefault();
	void deletePaths();

private slots:
	void onStopperFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void onConnmanStarting(int exitCode, QProcess::ExitStatus exitStatus);
public:
	explicit NetworkResetter(VeQItemSettings *settings, QObject *parent = nullptr);

	void resetNetwork();

signals:

};

#endif // NETWORKRESETTER_H
