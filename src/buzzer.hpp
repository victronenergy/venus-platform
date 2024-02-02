#pragma once

#include <QObject>

class VeQItem;

class Buzzer : public QObject
{
	Q_OBJECT
public:
	Buzzer(const QString &buzzerPath, QObject *parent = 0);
	~Buzzer();

	bool isBeeping() const;
	void buzzerOn();
	void buzzerOff();

private slots:
	void onItemStateChanged();

private:
	void setBuzzer(bool on);

	bool mBuzzerToggle;
	VeQItem *mBuzzerItem;
};
