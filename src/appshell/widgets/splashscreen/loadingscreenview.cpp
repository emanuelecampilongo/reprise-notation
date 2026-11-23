/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "loadingscreenview.h"

#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>

#include "translation.h"
#include "muse_framework_config.h"

using namespace mu::appshell;

static const QString imagePath(":/resources/LoadingScreen.svg");

static constexpr QSize loadingScreenSize(800, 380);

static const QColor versionNumberColor("#685B50");
static constexpr QRectF versionNumberRect(545, 327, 206, 28);

LoadingScreenView::LoadingScreenView(QWidget* parent)
    : QWidget(parent), muse::Contextable(muse::iocCtxForQWidget(this)),
    m_backgroundRenderer(new QSvgRenderer(imagePath, this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(loadingScreenSize);
}

bool LoadingScreenView::event(QEvent* event)
{
    if (event->type() == QEvent::Paint) {
        QPainter painter(this);
        painter.setLayoutDirection(layoutDirection());
        draw(&painter);
    }

    return QWidget::event(event);
}

void LoadingScreenView::draw(QPainter* painter)
{
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    // Draw background
    m_backgroundRenderer->render(painter);

    // Draw Reprise's public version independently from the inherited MuseScore
    // engine version, which remains necessary for binary compatibility.
    QFont font(QString::fromStdString(uiConfiguration()->fontFamily()));
    font.setPixelSize(14);
    painter->setFont(font);
    painter->setPen(versionNumberColor);
    painter->drawText(versionNumberRect, Qt::AlignRight | Qt::AlignVCenter,
                      muse::qtrc("appshell", "Version %1").arg(QStringLiteral(REPRISE_APP_VERSION)));
}
