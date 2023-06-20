#include "time.hpp"

#include <unistd.h>
#include <time.h>

#include <QDateTime>
#include <QObject>
#include <QtGlobal>

int VeQItemTime::setValue(const QVariant &value)
{
	bool ok;
	quint64 secSinceEpoch = value.toULongLong(&ok);
	if (!ok)
		return -1;

	struct timespec tv;
	tv.tv_sec = secSinceEpoch;
	tv.tv_nsec = 0;

	clock_settime(CLOCK_REALTIME, &tv);

	system("hwclock --utc --systohc");

	// update the timestamp file as well, so time can be set backwards with a power cycle
	system("/etc/init.d/save-rtc.sh");

	sync();

	produceValue(getValue());

	return 0;
}

QVariant VeQItemTime::getValue()
{
	struct timespec tv;
	if (clock_gettime(CLOCK_REALTIME, &tv) != 0)
		return QVariant();
	return QVariant::fromValue<quint64>(tv.tv_sec);

}

QString VeQItemTime::getText()
{
	QDateTime time = QDateTime::fromMSecsSinceEpoch(getValue().toLongLong() * 1000);
	return time.toString("yyyy-MM-dd hh:mm 'UTC'");
}
