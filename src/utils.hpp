#pragma once

#include <veutil/qt/ve_qitems_dbus.hpp>

void setWifiHotspotAndBluetooth(VeQItemSettings *settings, const int val);
bool serviceRunning(QString const &svc, bool *ok);
bool stopServiceAndWaitForExit(const QString &svc, const std::chrono::seconds timeout = std::chrono::seconds(10));
