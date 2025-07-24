#ifndef BUTTONHANDLER_H
#define BUTTONHANDLER_H

#include <QObject>
#include <vector>
#include <unordered_map>
#include <QSocketNotifier>

#include <libevdev/libevdev.h>

class LibevdevDevice
{
	Q_DISABLE_COPY(LibevdevDevice);

	int fd = -1;
	struct libevdev *dev = nullptr;

	void free() noexcept;
public:
	enum class KeyEvent
	{
		Up,
		Down
	};


	LibevdevDevice() = default;
	LibevdevDevice(const QString &devpath);
	LibevdevDevice(LibevdevDevice &&other) noexcept;
	~LibevdevDevice() noexcept;

	LibevdevDevice &operator=(LibevdevDevice &&other) noexcept;

	static std::vector<LibevdevDevice> getDevices();
	QString getName() const;
	bool isButton() const;
	int getFd() const;
	QList<KeyEvent> getEvents();
};

template<class T> auto privateOverload(void ( QSocketNotifier::* s)( QSocketDescriptor,QSocketNotifier::Type,T ) ){return s;}


class ButtonHandler : public QObject
{
	Q_OBJECT

	std::unordered_map<int, LibevdevDevice> mButtons;

private slots:
	void onButtonActivity(QSocketDescriptor socket, QSocketNotifier::Type type);

public:
	ButtonHandler(QObject *parent);

signals:
	void onShortPress();
	void onDoublePress();
	void onLongPress();
};

#endif // BUTTONHANDLER_H
