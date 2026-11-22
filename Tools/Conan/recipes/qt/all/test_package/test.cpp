// Copyright (c) Contributors to the Open 3D Engine Project.
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>

#ifdef __EMSCRIPTEN__
#include <cstdio>

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtSvg/QSvgRenderer>
#endif

int main()
{
    if (QByteArray(qVersion()) != QT_VERSION_STR)
    {
        return 1;
    }
#ifdef __EMSCRIPTEN__
    static_assert(sizeof(void*) == QT_TEST_POINTER_SIZE);
    QImage image(1, 1, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QSvgRenderer svg(QByteArrayLiteral(
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1 1"><rect width="1" height="1" fill="#ff0000"/></svg>)"));
    svg.render(&painter);
    painter.end();
    if (image.pixelColor(0, 0) != QColor(Qt::red))
    {
        return 1;
    }
    std::puts("Qt WebAssembly SVG rendering passed");
#endif
    return 0;
}
