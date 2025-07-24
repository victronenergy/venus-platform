#include "buttonhandler.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <QDir>

void LibevdevDevice::free() noexcept
{
	if (dev)
	{
		libevdev_free(dev);
		dev = nullptr;
	}

	if (fd >= 0)
	{
		close(fd);
		fd = -1;
	}
}

LibevdevDevice::LibevdevDevice(const QString &devpath)
{
	fd = open(devpath.toStdString().c_str(), O_RDONLY|O_NONBLOCK);

	if (fd < 0) {
		qCritical() << "LibevdevDevice: can't open" << devpath;
		return;
	}

	int rc = libevdev_new_from_fd(fd, &dev);
	if (rc < 0) {
		close(fd);
		dev = nullptr;
		qCritical() << "Failed to init libevdev: " << strerror(-rc);
		return;
	}

	qDebug() << "Initializing LibevdevDevice from" << devpath << ":" << getName();
}

LibevdevDevice::LibevdevDevice(LibevdevDevice &&other) noexcept
{
	*this = std::move(other);
}

LibevdevDevice::~LibevdevDevice() noexcept
{
	free();
}

LibevdevDevice &LibevdevDevice::operator=(LibevdevDevice &&other) noexcept
{
	free();

	this->dev = other.dev;
	this->fd = other.fd;
	other.fd = -1;
	other.dev = nullptr;
	return *this;
}

std::vector<LibevdevDevice> LibevdevDevice::getDevices()
{
	std::vector<LibevdevDevice> result;

	const QString devpath("/dev/input/");

	QDir devInput(devpath);
	devInput.setFilter(QDir::System | QDir::Files);
	QStringList dev_paths = devInput.entryList({"event*"});

	for (QString &d : dev_paths) {
		d.prepend(devpath);
		result.emplace_back(d);
	}

	return result;
}

QString LibevdevDevice::getName() const
{
	if (!dev)
		return "unknown device";

	return libevdev_get_name(dev);
}

bool LibevdevDevice::isButton() const
{
	if (!dev)
		return false;

	// KEY_CONFIG is just some weird unusual button that was decided for the GX button.
	return libevdev_has_event_code(dev, EV_KEY, KEY_CONFIG);
}

int LibevdevDevice::getFd() const
{
	return fd;
}

/**
 * Is usually just one event.
 */
QList<LibevdevDevice::KeyEvent> LibevdevDevice::getEvents()
{
	QList<KeyEvent> result;

	if (!dev)
		return result;

	int guard = 0;
	int rc = 0;
	do {
		struct input_event ev;
		memset(&ev, 0, sizeof(ev));

		// TODO: what about that sync stuff  https://www.freedesktop.org/software/libevdev/doc/latest/group__events.html#gabb96c864e836c0b98788f4ab771c3a76
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (rc == 0)
		{
			if (ev.type == EV_KEY && ev.code == KEY_CONFIG)
			{
				KeyEvent event = static_cast<KeyEvent>(ev.value);
				result.push_back(event);
			}
		}
	} while ((rc == 1 || rc == 0 || rc == -EAGAIN) && guard++ < 1000);

	return result;
}

void ButtonHandler::resetPressState()
{
	mLongPress.stop();
	mShortPresses.stop();
	mCurState = LibevdevDevice::KeyEvent::Up;
	mPressCounter = 0;
}

void ButtonHandler::onShortPressesTimeout()
{
	if (mCurState == LibevdevDevice::KeyEvent::Down)
		return;

	if (mPressCounter == 1)
	{
		qDebug() << "shortPress";
		resetPressState();
		emit shortPress();
	}
	else if (mPressCounter == 2)
	{
		qDebug() << "doublePress";
		resetPressState();
		emit doublePress();
	}
	else
	{
		resetPressState();
	}
}

void ButtonHandler::onLongPresTimeout()
{
	qDebug() << "long";
	resetPressState();
	emit longPress();
}

void ButtonHandler::onButtonActivity(QSocketDescriptor socket, QSocketNotifier::Type type)
{
	(void)type;

	auto pos = mButtons.find(socket);
	if (pos == mButtons.end())
		return;

	LibevdevDevice &dev = pos->second;
	QList<LibevdevDevice::KeyEvent> events = dev.getEvents();

	for (LibevdevDevice::KeyEvent event : events)
	{
		mCurState = event;

		if (event == LibevdevDevice::KeyEvent::Up)
		{
			mLongPress.stop();

			// Reset on aborted long press while retaining tracking of short presses.
			if (!mShortPresses.isActive())
				resetPressState();

			continue;
		}

		mPressCounter++;
		mShortPresses.start();
		mLongPress.start();
	}
}

ButtonHandler::ButtonHandler(QObject *parent) :
	QObject(parent)
{
	mShortPresses.setSingleShot(true);
	mShortPresses.setInterval(750);
	mLongPress.setSingleShot(true);
	mLongPress.setInterval(4000);

	connect(&mShortPresses, &QTimer::timeout, this, &ButtonHandler::onShortPressesTimeout);
	connect(&mLongPress, &QTimer::timeout, this, &ButtonHandler::onLongPresTimeout);

	std::vector<LibevdevDevice> all_devices = LibevdevDevice::getDevices();

	for (LibevdevDevice &d : all_devices) {
		if (d.isButton()) {
			qDebug() << "Using" << d.getName() << "as GX button";

			const int fd = d.getFd();
			QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);

			connect(
				notifier,
				privateOverload(&QSocketNotifier::activated),
				this,
				&ButtonHandler::onButtonActivity);

			mButtons[fd] = std::move(d);
		}
	}
}



