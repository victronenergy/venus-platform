#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QDBusMessage>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QString>
#include <QTextStream>

#include <veutil/qt/ve_dbus_connection.hpp>

#include "token_users.hpp"

TokenUser TokenUser::fromJson(const QJsonObject &json)
{
	TokenUser ret;

	if (const QJsonValue v = json["token_name"]; v.isString())
		ret.mTokenName = v.toString();
	else
		return ret;

	if (const QJsonValue v = json["password_hash"]; v.isString())
		ret.mPasswordHash = v.toString();

	return ret;
}

QJsonObject TokenUser::toJson(bool withHash) const
{
	QJsonObject json;
	json["token_name"] = mTokenName;
	if (withHash)
		json["password_hash"] = mPasswordHash;
	return json;
}

QDebug operator<<(QDebug debug, const TokenUser &user)
{
	QDebugStateSaver saver(debug);
	debug.nospace() << "{\"token\": " << user.tokenname() << ", \"pwd\": " << user.passwordHash() << '}';
	return debug;
}

bool TokenUsers::fromJson(const QJsonArray &users)
{
	mTokenUsers.clear();
	for (const QJsonValue &user: users)
		mTokenUsers.append(TokenUser::fromJson(user.toObject()));

	return true;
}

QJsonArray TokenUsers::toJson(bool withHash) const
{
	QJsonArray array;
	for (TokenUser const &user: mTokenUsers)
		array.append(user.toJson(withHash));

	return array;
}

bool TokenUsers::load(QString const &filename)
{
	QFile config(filename);
	if (!config.exists())
		return true;

	if (!config.open(QFile::ReadOnly)) {
		qCritical() << "could not open" << filename;
		return false;
	}

	QByteArray data = config.readAll();
	QJsonDocument doc(QJsonDocument::fromJson(data));
	return fromJson(doc.array());
}

bool TokenUsers::save(QString const &filename)
{
	QSaveFile saveFile(filename);

	if (!saveFile.open(QIODevice::WriteOnly)) {
		qWarning("Couldn't open token file for saving.");
		return false;
	}

	QJsonArray object = toJson();
	saveFile.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
	saveFile.commit();

	return true;
}

int TokenUsers::removeTokenUser(const QString &username)
{
	QMutableListIterator<TokenUser> i(mTokenUsers);
	int n = 0;

	while (i.hasNext()) {
		TokenUser user = i.next();
		if (user.tokenname() == username) {
			i.remove();
			n++;
		}
	}

	return n;
}

bool TokenUsers::addTokenUser(const TokenUser &user)
{
	if (!user.isValid()) {
		qCritical() << user << "is invalid";
		return false;
	}

	removeTokenUser(user.tokenname());
	mTokenUsers.append(user);

	return true;
}

char const *tokendir = "/var/volatile/tokens";
char const *tokenfile = "/data/conf/tokens.json";

TokenUserWatcher::TokenUserWatcher(VeQItem *platform) : QObject()
{
	mTokensItem = platform->itemGetOrCreate("Tokens/Users");
	platform->itemGet("Tokens")->itemAddChild("Remove", new TokenRemoveItem(this));

	QDir dir(tokendir);
	if (!dir.exists()) {
		dir.mkpath(".");
		chmod(tokendir, S_IRUSR | S_IWUSR | S_IXUSR);
		struct passwd * pw = getpwnam("php-fpm");
		if (pw)
			chown(tokendir, pw->pw_uid, pw->pw_gid);
		else
			qCritical() << "user php-fpm not found";
	}

	mWatcher.addPath(tokendir);
	connect(&mWatcher, &QFileSystemWatcher::directoryChanged, this, &TokenUserWatcher::scanDirectory);
	scanDirectory();
	updateTokens();
}

TokenUserWatcher::~TokenUserWatcher()
{
}

void TokenUserWatcher::scanDirectory()
{
	QDir directory(tokendir);
	for (QString const &filename: directory.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
		QFile json(directory.filePath(filename));
		if (json.open(QFile::ReadOnly)) {
			QByteArray data = json.readAll();
			QJsonDocument doc(QJsonDocument::fromJson(data));
			TokenUser user = TokenUser::fromJson(doc.object());

			if (user.isValid()) {
				TokenUsers users;

				if (!users.load(tokenfile))
					qCritical() << "failed to load token users";

				if (users.addTokenUser(user)) {
					users.save(tokenfile);
					qInfo() << "adding token" << user;
					updateTokens();
				}
			} else {
				qCritical() << "ignoring invalid token" << user;
			}
			json.remove();
		} else {
			qCritical() << "could not open" << filename;
		}
	}
}

void TokenUserWatcher::updateTokens()
{
	TokenUsers users;
	users.load(tokenfile);
	QJsonArray array = users.toJson(false);
	QJsonDocument doc(array);
	QString json = doc.toJson(QJsonDocument::Compact);
	mTokensItem->produceValue(json);
}

int TokenRemoveItem::setValue(const QVariant &value)
{
	QString token = value.toString();
	qDebug() << "removing" << token;

	TokenUsers users;
	if (!users.load(tokenfile)) {
		qCritical() << "failed to load token users";
		return -1;
	}

	int count = users.removeTokenUser(token);
	if (count == 0) {
		qDebug() << "user" << token << "doesn't exist";
		return -3;
	}

	if (!users.save(tokenfile)) {
		qCritical() << "failed to save token users";
		return -2;
	}

	mWatcher->updateTokens();

	QDBusMessage signal = QDBusMessage::createSignal("/Tokens/Users", "com.victronenergy.TokenUsers", "UserRemoved");
	signal << token;
	if (!VeDbusConnection::getConnection().send(signal))
		qCritical() << "failed to send token remove signal";

	return 0;
}
