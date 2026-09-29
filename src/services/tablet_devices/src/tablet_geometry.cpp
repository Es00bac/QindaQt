// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_geometry.h>

#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace QindaQt::Services::TabletDevices {
namespace {

constexpr double Epsilon = 1e-9;

// KWin serializes a QMatrix4x4 row by row as plain numbers; row 0 and row 1
// carry the affine, row 2 and row 3 stay identity (kwin 6.6.6
// src/backends/libinput/device.cpp serializeMatrix/getMatrix).
QString serializeAffine(double a, double b, double c, double d, double e,
                        double f) {
    const std::array<double, 16> values{a,   b,   c,   0.0, d,   e,
                                        f,   0.0, 0.0, 0.0, 1.0, 0.0,
                                        0.0, 0.0, 0.0, 1.0};
    QStringList parts;
    parts.reserve(int(values.size()));
    for (const double value : values) {
        // AGENT-GUARD: QString::number's default six significant digits lose
        // enough of a calibration coefficient to move the pen a pixel or two
        // at the edges. KWin parses each value with toFloat, so nine digits
        // cost nothing and keep the fit exact to float precision.
        parts.append(QString::number(value, 'g', 9));
    }
    return parts.join(QLatin1Char(','));
}

// Solves the 3x3 normal equations of the least-squares affine fit for one
// output coordinate. Returns false when the system is singular, which is
// exactly the collinear/coincident measurement case.
bool solveAffineRow(const std::array<std::array<double, 3>, 3> &normal,
                    const std::array<double, 3> &rhs,
                    std::array<double, 3> *out) {
    std::array<std::array<double, 4>, 3> augmented{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            augmented[row][column] = normal[row][column];
        }
        augmented[row][3] = rhs[row];
    }
    for (std::size_t pivot = 0; pivot < 3; ++pivot) {
        std::size_t best = pivot;
        for (std::size_t row = pivot + 1; row < 3; ++row) {
            if (std::abs(augmented[row][pivot]) >
                std::abs(augmented[best][pivot])) {
                best = row;
            }
        }
        if (std::abs(augmented[best][pivot]) < Epsilon) {
            return false;
        }
        std::swap(augmented[pivot], augmented[best]);
        for (std::size_t row = 0; row < 3; ++row) {
            if (row == pivot) {
                continue;
            }
            const double factor = augmented[row][pivot] / augmented[pivot][pivot];
            for (std::size_t column = pivot; column < 4; ++column) {
                augmented[row][column] -= factor * augmented[pivot][column];
            }
        }
    }
    for (std::size_t row = 0; row < 3; ++row) {
        (*out)[row] = augmented[row][3] / augmented[row][row];
    }
    return true;
}

} // namespace

QString identityCalibrationMatrix() {
    return serializeAffine(1.0, 0.0, 0.0, 0.0, 1.0, 0.0);
}

QString calibrationMatrixFor(const QList<QPointF> &measured,
                             const QList<QPointF> &targets) {
    if (measured.size() != targets.size() || measured.size() < 3) {
        return {};
    }
    for (const QPointF &point : measured) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
            return {};
        }
    }
    for (const QPointF &point : targets) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
            return {};
        }
    }
    // Normal equations of  [x y 1] * [a d; b e; c f] = [tx ty]
    std::array<std::array<double, 3>, 3> normal{};
    std::array<double, 3> rhsX{};
    std::array<double, 3> rhsY{};
    for (qsizetype index = 0; index < measured.size(); ++index) {
        const std::array<double, 3> row{measured.at(index).x(),
                                        measured.at(index).y(), 1.0};
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                normal[i][j] += row[i] * row[j];
            }
            rhsX[i] += row[i] * targets.at(index).x();
            rhsY[i] += row[i] * targets.at(index).y();
        }
    }
    std::array<double, 3> solutionX{};
    std::array<double, 3> solutionY{};
    if (!solveAffineRow(normal, rhsX, &solutionX) ||
        !solveAffineRow(normal, rhsY, &solutionY)) {
        return {};
    }
    // AGENT-GUARD: A fit whose linear part collapses would map the whole
    // tablet onto a line and lose the pen. Refuse it like a singular solve.
    const double determinant =
        solutionX[0] * solutionY[1] - solutionX[1] * solutionY[0];
    if (std::abs(determinant) < 1e-6) {
        return {};
    }
    return serializeAffine(solutionX[0], solutionX[1], solutionX[2],
                           solutionY[0], solutionY[1], solutionY[2]);
}

TabletArea letterboxArea(double tabletAspect, double outputWidth,
                         double outputHeight) {
    if (!(tabletAspect > 0.0) || !(outputWidth > 0.0) ||
        !(outputHeight > 0.0)) {
        return {};
    }
    const double outputAspect = outputWidth / outputHeight;
    if (std::abs(outputAspect - tabletAspect) < 1e-6) {
        return {};
    }
    if (tabletAspect > outputAspect) {
        // The tablet is wider than the screen: keep full width, shrink
        // height and center it.
        const double height = outputAspect / tabletAspect;
        return TabletArea{0.0, (1.0 - height) / 2.0, 1.0, height};
    }
    const double width = tabletAspect / outputAspect;
    return TabletArea{(1.0 - width) / 2.0, 0.0, width, 1.0};
}

std::optional<TabletArea> normalizedArea(const TabletArea &area,
                                         double minimumExtent) {
    // A rotation or a QML drag overshoots an edge by a few ulps; anything
    // further out than this is a real request outside the surface.
    constexpr double SnapTolerance = 1e-6;
    if (!std::isfinite(area.x) || !std::isfinite(area.y) ||
        !std::isfinite(area.width) || !std::isfinite(area.height)) {
        return std::nullopt;
    }
    TabletArea result = area;
    if (result.x < 0.0) {
        if (result.x < -SnapTolerance) {
            return std::nullopt;
        }
        result.width += result.x;
        result.x = 0.0;
    }
    if (result.y < 0.0) {
        if (result.y < -SnapTolerance) {
            return std::nullopt;
        }
        result.height += result.y;
        result.y = 0.0;
    }
    // AGENT-GUARD: recompute the far extent as 1 - origin rather than
    // clamping x + width. x + (1 - x) rounds to at most 1.0 in IEEE double
    // (the error of 1 - x is at most half an ulp below 1, and the tie rounds
    // to the even 1.0), which is exactly the x2 <= 1.0 libinput checks.
    if (result.x + result.width > 1.0) {
        if (result.x + result.width > 1.0 + SnapTolerance) {
            return std::nullopt;
        }
        result.width = 1.0 - result.x;
    }
    if (result.y + result.height > 1.0) {
        if (result.y + result.height > 1.0 + SnapTolerance) {
            return std::nullopt;
        }
        result.height = 1.0 - result.y;
    }
    const double minimum = std::max(minimumExtent, 0.0);
    // x1 < x2 must hold in double arithmetic too, or libinput refuses it.
    if (!(result.width >= minimum) || !(result.height >= minimum) ||
        !(result.x + result.width > result.x) ||
        !(result.y + result.height > result.y)) {
        return std::nullopt;
    }
    return result;
}

bool sameArea(const TabletArea &first, const TabletArea &second,
              double tolerance) {
    return std::abs(first.x - second.x) <= tolerance &&
           std::abs(first.y - second.y) <= tolerance &&
           std::abs(first.width - second.width) <= tolerance &&
           std::abs(first.height - second.height) <= tolerance;
}

double proportionalOutputAspect(const TabletArea &input, double tabletWidth,
                                double tabletHeight, double surfaceWidth,
                                double surfaceHeight) {
    if (!(input.width > 0.0) || !(input.height > 0.0) ||
        !(tabletWidth > 0.0) || !(tabletHeight > 0.0) ||
        !(surfaceWidth > 0.0) || !(surfaceHeight > 0.0)) {
        return 0.0;
    }
    // The physical width/height of the used part of the tablet, then the
    // normalized ratio that draws that same shape on this surface.
    const double physical =
        (input.width * tabletWidth) / (input.height * tabletHeight);
    const double aspect = physical * surfaceHeight / surfaceWidth;
    return std::isfinite(aspect) && aspect > 0.0 ? aspect : 0.0;
}

TabletArea proportionalOutputArea(const TabletArea &input, double tabletWidth,
                                  double tabletHeight,
                                  const TabletArea &output,
                                  double surfaceWidth, double surfaceHeight) {
    const double aspect = proportionalOutputAspect(
        input, tabletWidth, tabletHeight, surfaceWidth, surfaceHeight);
    if (!(aspect > 0.0) || !(output.width > 0.0) || !(output.height > 0.0)) {
        return output;
    }
    // Keep the rectangle's area and centre and change only its shape, so
    // repeated edits of the tablet side never shrink the screen side; then
    // shrink uniformly until it fits the surface.
    double height = std::sqrt((output.width * output.height) / aspect);
    double width = aspect * height;
    const double fit = std::min({1.0, 1.0 / width, 1.0 / height});
    width = std::min(1.0, width * fit);
    height = std::min(1.0, height * fit);
    const double centreX = output.x + output.width / 2.0;
    const double centreY = output.y + output.height / 2.0;
    const double left =
        std::max(0.0, std::min(centreX - width / 2.0, 1.0 - width));
    const double top =
        std::max(0.0, std::min(centreY - height / 2.0, 1.0 - height));
    return TabletArea{left, top, width, height};
}

bool isValidPressureCurve(const QString &curve) {
    const QStringList pairs =
        curve.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (pairs.size() < 2) {
        return false;
    }
    double previousX = -1.0;
    for (const QString &pair : pairs) {
        const QStringList parts = pair.split(QLatin1Char(','));
        if (parts.size() != 2) {
            return false;
        }
        bool xOk = false;
        bool yOk = false;
        const double x = parts.at(0).toDouble(&xOk);
        const double y = parts.at(1).toDouble(&yOk);
        if (!xOk || !yOk || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0 ||
            x <= previousX) {
            return false;
        }
        previousX = x;
    }
    return true;
}

QString pressureCurveForThreshold(double threshold) {
    const double clamped = std::isfinite(threshold)
                               ? std::min(std::max(threshold, 0.0), 0.9)
                               : 0.0;
    // KWin reads exactly two control points and always adds (1,1) as the
    // end point, so the curve is "start;control;" in its serialization.
    return QStringLiteral("%1,0;1,1;")
        .arg(QString::number(clamped));
}

} // namespace QindaQt::Services::TabletDevices
