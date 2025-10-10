#ifndef NETWORKRESETTER_H
#define NETWORKRESETTER_H

#include <QObject>
#include <list>
#include <QProcess>
#include <QTimer>
#include <chrono>
#include <veutil/qt/ve_qitem.hpp>


class NetworkResetter : public QObject
{
	Q_OBJECT

	VeQItemSettings *mSettings = nullptr;

	std::list<QProcess> mStopProcesses;
	QTimer mLocalSettingsChecker;
	std::chrono::time_point<std::chrono::steady_clock> mWaitForLocalSettingsEndPoint;

	void initiate();
	void stopRelevantServices();
	void makeLEDsIndicateReset();
	void setSettingsToDefault();
	void deletePaths();
	void initiateSettingsShutdownAndReboot();

private slots:
	void onStopperFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void onlocalSettingsCheckerTimeout();
public:
	explicit NetworkResetter(VeQItemSettings *settings, QObject *parent = nullptr);

	void resetNetwork();

signals:

};

#endif // NETWORKRESETTER_H
