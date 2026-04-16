#include "introoverlay.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QScreen>
#include <QGuiApplication>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

// ============================================================
// 动画时间线 (~60fps, 150帧 ≈ 2.5秒)
//
// Phase 1 (0-36帧, 0-0.6s): 背景圆角矩形 + 菱形1 描边绘制
// Phase 2 (18-54帧, 0.3-0.9s): 菱形2 描边绘制 (与菱形1重叠)
// Phase 3 (54-84帧, 0.9-1.4s): 菱形填充渐显 (从描边到实心)
// Phase 4 (84-102帧, 1.4-1.7s): 保持
// Phase 5 (102-150帧, 1.7-2.5s): 淡出
// ============================================================

IntroOverlay::IntroOverlay(QWidget *parent)
    : QWidget(parent)
{
    // 菱形1顶点 (原始坐标, 中心约 12,14)
    m_diamond1 = QPolygonF({
        QPointF(7, 14), QPointF(12, 9), QPointF(17, 14), QPointF(12, 19)
    });
    // 菱形2顶点 (五边形)
    m_diamond2 = QPolygonF({
        QPointF(13, 14), QPointF(18, 9), QPointF(21, 12), QPointF(21, 16), QPointF(18, 19)
    });
}

void IntroOverlay::startAnimation()
{
    m_frame = 0;
    m_timer.start(16, this);
    show();
    raise();
    setFocus();
}

void IntroOverlay::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timer.timerId()) {
        m_frame++;
        update();
        if (m_frame >= TOTAL_FRAMES) {
            m_timer.stop();

            // 用 QPropertyAnimation 做最后的平滑淡出
            auto *effect = new QGraphicsOpacityEffect(this);
            effect->setOpacity(1.0);
            setGraphicsEffect(effect);

            auto *fadeOut = new QPropertyAnimation(effect, "opacity");
            fadeOut->setDuration(300);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            fadeOut->setEasingCurve(QEasingCurve::InOutCubic);
            connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
                hide();
                deleteLater();
                emit animationFinished();
            });
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
}

void IntroOverlay::resizeEvent(QResizeEvent *)
{
    int w = width() * SCALE;
    int h = height() * SCALE;
    if (m_buffer.width() != w || m_buffer.height() != h) {
        m_buffer = QPixmap(w, h);
    }
}

void IntroOverlay::paintEvent(QPaintEvent *)
{
    int w = width(), h = height();
    if (w <= 0 || h <= 0) return;

    int bw = w * SCALE;
    int bh = h * SCALE;
    if (m_buffer.width() != bw || m_buffer.height() != bh) {
        m_buffer = QPixmap(bw, bh);
    }
    m_buffer.fill(Qt::transparent);

    QPainter buf(&m_buffer);
    buf.setRenderHint(QPainter::Antialiasing, true);

    // 2x 缩放
    buf.scale(SCALE, SCALE);

    double t = qBound(0.0, m_frame / double(TOTAL_FRAMES), 1.0);

    // ---- 背景渐变 (与左面板一致) ----
    QLinearGradient grad(0, 0, w * 0.8, h);
    grad.setColorAt(0, QColor("#3D2596"));
    grad.setColorAt(1, QColor("#2d1a72"));

    double bgOpacity = easeOutCubic(qBound(0.0, t / 0.15, 1.0));
    buf.setOpacity(bgOpacity);
    buf.fillRect(rect(), grad);
    buf.setOpacity(1.0);

    // ---- 图标参数 ----
    double iconScale = qMin(w, h) / 32.0 * 1.8; // 自适应缩放
    double cx = w / 2.0;
    double cy = h / 2.0;

    // Phase 1: 背景圆角矩形 (0-0.3s)
    double bgRectProgress = easeOutBack(qBound(0.0, t / 0.2, 1.0));
    if (bgRectProgress > 0) {
        buf.save();
        buf.translate(cx, cy);
        double s = bgRectProgress;
        buf.scale(s, s);

        double halfSize = 14 * iconScale;
        double radius = 7 * iconScale;

        QPainterPath bgPath;
        bgPath.addRoundedRect(QRectF(-halfSize, -halfSize, halfSize * 2, halfSize * 2),
                              radius / 2.0, radius / 2.0);
        buf.setBrush(QColor(255, 255, 255, 38));
        buf.setPen(Qt::NoPen);
        buf.drawPath(bgPath);
        buf.restore();
    }

    // Phase 1: 菱形1 描边 (0.1-0.6s)
    double d1DrawProgress = easeInOutCubic(qBound(0.0, (t - 0.1) / 0.5, 1.0));
    if (d1DrawProgress > 0) {
        drawDiamondOutline(buf, m_diamond1, d1DrawProgress, cx, cy, iconScale);
    }

    // Phase 2: 菱形2 描边 (0.25-0.75s)
    double d2DrawProgress = easeInOutCubic(qBound(0.0, (t - 0.25) / 0.5, 1.0));
    if (d2DrawProgress > 0) {
        drawDiamondOutline(buf, m_diamond2, d2DrawProgress, cx, cy, iconScale);
    }

    // Phase 3: 填充渐显 (0.6-1.0s)
    double fillProgress = easeOutCubic(qBound(0.0, (t - 0.6) / 0.4, 1.0));
    if (fillProgress > 0) {
        drawDiamondFill(buf, m_diamond1, fillProgress, cx, cy, iconScale,
                        QColor(255, 255, 255, 230));  // 90% 不透明
        drawDiamondFill(buf, m_diamond2, fillProgress, cx, cy, iconScale,
                        QColor(255, 255, 255, 128));  // 50% 不透明
    }

    buf.end();

    // 绘制到 widget
    QPainter p(this);
    p.drawPixmap(0, 0, m_buffer);
}

// ==================== 辅助函数 ====================

double IntroOverlay::easeOutCubic(double t)
{
    return 1.0 - qPow(1.0 - t, 3.0);
}

double IntroOverlay::easeOutBack(double t)
{
    double c1 = 1.70158;
    double c3 = c1 + 1.0;
    return 1.0 + c3 * qPow(t - 1.0, 3.0) + c1 * qPow(t - 1.0, 2.0);
}

double IntroOverlay::easeInOutCubic(double t)
{
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - qPow(-2.0 * t + 2.0, 3.0) / 2.0;
}

QPointF IntroOverlay::lerpPoint(const QPointF &a, const QPointF &b, double t)
{
    return a + (b - a) * t;
}

void IntroOverlay::drawDiamondOutline(QPainter &p, const QPolygonF &diamond,
                                       double progress, double offsetX, double offsetY, double scale)
{
    int n = diamond.size();
    if (n < 2) return;

    double totalEdges = n; // 包括闭合边
    double progressEdges = progress * totalEdges;
    int completedEdges = int(progressEdges);
    double edgeFrac = progressEdges - completedEdges;

    // 先将顶点坐标转换到屏幕坐标 (以 offset 为中心, scale 缩放)
    // 原始 viewBox 中心约 (14, 14)
    auto transform = [&](const QPointF &pt) -> QPointF {
        return QPointF(offsetX + (pt.x() - 14.0) * scale,
                      offsetY + (pt.y() - 14.0) * scale);
    };

    QPen pen(QColor(255, 255, 255, 200));
    pen.setWidthF(2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    QPainterPath path;

    if (completedEdges == 0) {
        // 正在画第一条边
        QPointF start = transform(diamond[0]);
        QPointF end = transform(diamond[1]);
        QPointF current = lerpPoint(start, end, edgeFrac);
        path.moveTo(start);
        path.lineTo(current);
    } else if (completedEdges < n) {
        // 已完成若干条边, 正在画下一条
        path.moveTo(transform(diamond[0]));
        for (int i = 1; i <= completedEdges; i++) {
            path.lineTo(transform(diamond[i % n]));
        }
        // 当前边部分绘制
        QPointF start = transform(diamond[completedEdges % n]);
        QPointF end = transform(diamond[(completedEdges + 1) % n]);
        QPointF current = lerpPoint(start, end, edgeFrac);
        path.lineTo(current);
    } else {
        // 全部画完
        path.moveTo(transform(diamond[0]));
        for (int i = 1; i < n; i++) {
            path.lineTo(transform(diamond[i]));
        }
        path.closeSubpath();
    }

    p.drawPath(path);
}

void IntroOverlay::drawDiamondFill(QPainter &p, const QPolygonF &diamond,
                                    double opacity, double offsetX, double offsetY, double scale,
                                    const QColor &color)
{
    auto transform = [&](const QPointF &pt) -> QPointF {
        return QPointF(offsetX + (pt.x() - 14.0) * scale,
                      offsetY + (pt.y() - 14.0) * scale);
    };

    QPolygonF screenPoly;
    for (const QPointF &pt : diamond) {
        screenPoly.append(transform(pt));
    }

    QColor fillColor = color;
    fillColor.setAlphaF(opacity * (color.alphaF()));
    p.setPen(Qt::NoPen);
    p.setBrush(fillColor);
    p.drawPolygon(screenPoly);
}
