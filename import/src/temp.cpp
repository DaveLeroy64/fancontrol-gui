/*
 * Copyright 2015  Malte Veerman <malte.veerman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "temp.h"

#include "hwmon.h"

#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QDir>

#include <KConfigCore/KSharedConfig>
#include <KConfigCore/KConfigGroup>
#include <KI18n/KLocalizedString>


#define TEST_HWMON_NAME "test"


namespace Fancontrol
{

Temp::Temp(uint index, Hwmon *parent, bool device) :
    Sensor(parent, index, QStringLiteral("temp"), device),
    m_valueStream(new QTextStream)
{
    if (!parent)
        return;

    auto path = device ? parent->path() + "/device" : parent->path();

    if (QDir(path).isReadable())
    {
        const auto valueFile = new QFile(path + "/temp" + QString::number(index) + "_input", this);
        const auto labelFile = new QFile(path + "/temp" + QString::number(index) + "_label");

        if (valueFile->open(QFile::ReadOnly))
        {
            m_valueStream->setDevice(valueFile);
            *m_valueStream >> m_value;
            m_value /= 1000;
        }
        else
        {
            delete valueFile;
            emit error(i18n("Can't open value file: \'%1\'", path + "/temp" + QString::number(index) + "_input"));
        }

        if (labelFile->exists())
        {
            if (labelFile->open(QFile::ReadOnly))
            {
                m_label = QTextStream(labelFile).readLine();
                setId(parent->name() + "/" + m_label);
            }
            else
                emit error(i18n("Can't open label file: \'%1\'", path + "/temp" + QString::number(index) + "_label"));
        }
        else
            emit error(i18n("Temp has no label: \'%1\'", path + "/temp" + QString::number(index)));

        delete labelFile;
    }
}

Temp::~Temp()
{
    auto device = m_valueStream->device();
    delete m_valueStream;
    delete device;
}

QString Temp::name() const
{
    const auto names = KSharedConfig::openConfig(QStringLiteral("fancontrol-gui"))->group("names");
    const auto localNames = names.group(parent() ? parent()->name() : QStringLiteral(TEST_HWMON_NAME));
    const auto name = localNames.readEntry("temp" + QString::number(index()), QString());

    if (name.isEmpty())
    {
        if (m_label.isEmpty())
            return "temp" + QString::number(index());

        return m_label;
    }
    return name;
}

void Temp::setName(const QString &name)
{
    const auto names = KSharedConfig::openConfig(QStringLiteral("fancontrol-gui"))->group("names");
    auto localNames = names.group(parent() ? parent()->name() : QStringLiteral(TEST_HWMON_NAME));

    if (name != localNames.readEntry("temp" + QString::number(index()), QString())
        && !name.isEmpty())
    {
        localNames.writeEntry("temp" + QString::number(index()), name);
        emit nameChanged();
    }
}

void Temp::toDefault()
{
    if (m_valueStream->device() && parent())
    {
        auto valueDevice = m_valueStream->device();
        m_valueStream->setDevice(Q_NULLPTR);
        delete valueDevice;

        auto path = device() ? parent()->path() + "/device" : parent()->path();

        if (QDir(path).isReadable())
        {
            const auto valueFile = new QFile(path + "/temp" + QString::number(index()) + "_input", this);

            if (valueFile->open(QFile::ReadOnly))
            {
                m_valueStream->setDevice(valueFile);
                *m_valueStream >> m_value;
                m_value /= 1000;
            }
            else
                emit error(i18n("Can't open value file: \'%1\'", valueFile->fileName()));
        }
    }
}

void Temp::update()
{
    auto success = false;

    m_valueStream->seek(0);
    const auto value = m_valueStream->readAll().toInt(&success) / 1000;

    if (!success)
        emit error(i18n("Can't update value of temp: \'%1\'", id()));

    if (value != m_value)
    {
        m_value = value;
        emit valueChanged();
    }
}

bool Temp::isValid() const
{
    return m_valueStream->device() || m_valueStream->string();
}

}
