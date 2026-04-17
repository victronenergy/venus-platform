#include "button_handler.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <QDir>

void LibevdevDevice::free() noexcept
{
	if (mDev) {
		libevdev_free(mDev);
		mDev = nullptr;
	}

	if (mFd >= 0) {
		close(mFd);
		mFd = -1;
	}
}

LibevdevDevice::LibevdevDevice(const QString &devpath)
{
	mFd = open(devpath.toStdString().c_str(), O_RDONLY | O_NONBLOCK);

	if (mFd < 0) {
		qCritical() << "[button] LibevdevDevice: can't open" << devpath;
		return;
	}

	int rc = libevdev_new_from_fd(mFd, &mDev);
	if (rc < 0) {
		close(mFd);
		mDev = nullptr;
		qCritical() << "[button] Failed to init libevdev: " << strerror(-rc);
		return;
	}

	qDebug() << "[button] Initializing LibevdevDevice from" << devpath << ":" << getName();
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

	this->mDev = other.mDev;
	this->mFd = other.mFd;
	other.mFd = -1;
	other.mDev = nullptr;
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
	if (!mDev)
		return "unknown device";

	return libevdev_get_name(mDev);
}

bool LibevdevDevice::isButton() const
{
	if (!mDev)
		return false;

	// KEY_CONFIG is just some weird unusual button that was decided for the GX button.
	return libevdev_has_event_code(mDev, EV_KEY, KEY_CONFIG);
}

int LibevdevDevice::getFd() const
{
	return mFd;
}

/**
 * Is usually just one event.
 */
QList<LibevdevDevice::KeyEvent> LibevdevDevice::getEvents()
{
	QList<KeyEvent> result;

	if (!mDev)
		return result;

	for (;;) {
		struct input_event ev = {};

		int rc = libevdev_next_event(mDev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (rc == -EAGAIN)
			break;

		if (rc < 0) {
			qCritical() << "[button] libevdev_next_event error" << rc << strerror(-rc);
			break;
		}

		if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
			if (ev.type == EV_KEY && ev.code == KEY_CONFIG) {
				KeyEvent event = static_cast<KeyEvent>(ev.value);
				result.push_back(event);
			}
		} else if (rc == LIBEVDEV_READ_STATUS_SYNC) {
			// Note: this sync return code will be returned if the linux event
			// queue ran over and libevdev needs to poll to create the events.
			// Since there is only one key in the device, just ignore this. Even
			// if it ever happens, continuing to read normally will simply empty
			// the sync queue.
			qWarning() << "[button] libevdev_next_event returned sync / overrun";
		} else {
			qCritical() << "[button] unknown return code" << rc;
		}
	}

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

	if (mPressCounter == 1) {
		qDebug() << "[button] shortPress";
		resetPressState();
		emit shortPress();
	} else if (mPressCounter == 2) {
		qDebug() << "[button] doublePress";
		resetPressState();
		emit doublePress();
	} else {
		resetPressState();
	}
}

void ButtonHandler::onLongPressTimeout()
{
	qDebug() << "[button] longPress";
	resetPressState();
	emit longPress();
}

void ButtonHandler::onButtonActivity(QSocketDescriptor socket)
{
	auto pos = mButtons.find(socket);
	if (pos == mButtons.end())
		return;

	LibevdevDevice &dev = pos->second;
	QList<LibevdevDevice::KeyEvent> events = dev.getEvents();

	for (LibevdevDevice::KeyEvent event : events) {
		mCurState = event;

		if (event == LibevdevDevice::KeyEvent::Up) {
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
	mShortPresses.setInterval(1500);
	mLongPress.setSingleShot(true);
	mLongPress.setInterval(4000);

	connect(&mShortPresses, &QTimer::timeout, this, &ButtonHandler::onShortPressesTimeout);
	connect(&mLongPress, &QTimer::timeout, this, &ButtonHandler::onLongPressTimeout);

	std::vector<LibevdevDevice> all_devices = LibevdevDevice::getDevices();

	for (LibevdevDevice &d : all_devices) {
		if (d.isButton()) {
			qDebug() << "[button] Using" << d.getName() << "as GX button";

			const int fd = d.getFd();
			QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);

			connect(
				notifier,
				&QSocketNotifier::activated,
				this,
				&ButtonHandler::onButtonActivity);

			mButtons[fd] = std::move(d);
		}
	}
}
