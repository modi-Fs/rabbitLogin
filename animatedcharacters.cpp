#include "animatedcharacters.h"
#include <QPainter>
#include <QPainterPath>
#include <QCursor>
#include <QtMath>
#include <QRandomGenerator>

// ============================================================
// 方案: 缓冲区 = widget大小×2，BASE坐标通过scale映射到整个widget
// BASE(0,0) → widget(0,0)
// BASE(450,500) → widget(width, height)
// 角色底部 = BASE_H=500 → widget底部 = height()  ✅
// 角色左边 = 0 → widget左边 = 0  ✅
// 角色右边 = 450 → widget右边 = width()  ✅
// ============================================================

AnimatedCharacters::AnimatedCharacters(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    initCharacters();

    // 16ms 动画循环 (~60fps)
    connect(&m_animTimer, &QTimer::timeout, this, &AnimatedCharacters::updateAnimation);
    m_animTimer.start(16);

    // 紫色和深灰角色随机眨眼
    scheduleBlink(m_purple);
    scheduleBlink(m_black);
}

void AnimatedCharacters::initCharacters()
{
    // ===== 紫色角色 (z-index:1) =====
    // 原版 CSS: left-[70px] w-[180px] h-[400px] (打字时 h-[440px])
    m_purple.left = 70;  m_purple.width = 180;  m_purple.height = 400;  m_purple.typingHeight = 440;
    m_purple.color = QColor("#6C3FF5");  m_purple.radius = 10;  m_purple.zIndex = 1;
    m_purple.hasEyeball = true;  m_purple.eyeSize = 18;  m_purple.pupilSize = 7;
    m_purple.maxPupilDist = 5;  m_purple.eyeGap = 32;
    m_purple.faceLeft = 45;  m_purple.faceTop = 40;  m_purple.faceSpeed = 0.06;

    // ===== 深灰角色 (z-index:2) =====
    // 原版 CSS: left-[240px] w-[120px] h-[310px]
    m_black.left = 240;  m_black.width = 120;  m_black.height = 310;  m_black.typingHeight = 310;
    m_black.color = QColor("#2D2D2D");  m_black.radius = 8;  m_black.zIndex = 2;
    m_black.hasEyeball = true;  m_black.eyeSize = 16;  m_black.pupilSize = 6;
    m_black.maxPupilDist = 4;  m_black.eyeGap = 24;
    m_black.faceLeft = 26;  m_black.faceTop = 32;  m_black.faceSpeed = 0.06;

    // ===== 橙色角色 (z-index:3) =====
    // 原版 CSS: left-0 w-[240px] h-[200px]
    m_orange.left = 0;  m_orange.width = 240;  m_orange.height = 200;  m_orange.typingHeight = 200;
    m_orange.color = QColor("#FF9B6B");  m_orange.radius = 120;  m_orange.zIndex = 3;
    m_orange.hasEyeball = false;  m_orange.pupilSize = 12;
    m_orange.maxPupilDist = 5;  m_orange.eyeGap = 32;
    m_orange.faceLeft = 82;  m_orange.faceTop = 90;  m_orange.faceSpeed = 0.25;

    // ===== 黄色角色 (z-index:4) =====
    // 原版 CSS: left-[310px] w-[140px] h-[230px]
    m_yellow.left = 310;  m_yellow.width = 140;  m_yellow.height = 230;  m_yellow.typingHeight = 230;
    m_yellow.color = QColor("#E8D754");  m_yellow.radius = 70;  m_yellow.zIndex = 4;
    m_yellow.hasEyeball = false;  m_yellow.pupilSize = 12;
    m_yellow.maxPupilDist = 5;  m_yellow.eyeGap = 24;
    m_yellow.faceLeft = 52;  m_yellow.faceTop = 40;  m_yellow.faceSpeed = 0.25;
    m_yellow.hasMouth = true;  m_yellow.mouthLeft = 40;  m_yellow.mouthTop = 88;  m_yellow.mouthWidth = 80;
}

// ==================== 坐标转换 ====================

QPointF AnimatedCharacters::widgetToBase(const QPointF &wp) const
{
    // widget坐标 → BASE坐标 (简单除法，因为 scale(sx,sy) 是线性映射)
    double sx = double(width()) / BASE_W;
    double sy = double(height()) / BASE_H;
    return QPointF(wp.x() / sx, wp.y() / sy);
}

// ==================== 公共接口 ====================

void AnimatedCharacters::setTyping(bool typing)
{
    if (m_typing == typing) return;
    m_typing = typing;
    if (typing) {
        m_looking = true;
        QTimer::singleShot(800, this, [this]() { m_looking = false; });
    } else {
        m_looking = false;
    }
}

void AnimatedCharacters::setShowPassword(bool show)
{
    if (m_showPassword == show) return;
    m_showPassword = show;
    if (show && m_passwordLength > 0) {
        m_looking = false;
        schedulePeek();
    } else if (!show) {
        m_peeking = false;
        m_peekTimer.stop();
    }
}

void AnimatedCharacters::setPasswordLength(int len)
{
    m_passwordLength = len;
}

// ==================== 动画循环 ====================

void AnimatedCharacters::updateAnimation()
{
    QPointF globalPos = QCursor::pos();
    QPointF widgetPos = mapFromGlobal(globalPos.toPoint());
    QPointF mouse = widgetToBase(widgetPos);
    double mx = mouse.x(), my = mouse.y();

    bool showing = isShowing();
    bool hiding = isHiding();
    bool looking = m_looking;

    // ===== 紫色角色 =====
    {
        if (showing) {
            m_purple.skewX.target = 0;
            m_purple.translateX.target = 0;
        } else if (looking || hiding) {
            double cx = m_purple.left + m_purple.width / 2.0;
            double r = mx - cx;
            double baseSkew = qBound(-6.0, -r / 120.0, 6.0);
            m_purple.skewX.target = baseSkew - 12;
            m_purple.translateX.target = 40;
        } else {
            double cx = m_purple.left + m_purple.width / 2.0;
            double r = mx - cx;
            m_purple.skewX.target = qBound(-6.0, -r / 120.0, 6.0);
            m_purple.translateX.target = 0;
        }

        if ((looking || hiding) && !showing) {
            m_purple.heightAnim.target = m_purple.typingHeight;
        } else {
            m_purple.heightAnim.target = m_purple.height;
        }

        if (showing) {
            m_purple.faceOffX.target = m_peeking ? 55 : 20;
            m_purple.faceOffY.target = m_peeking ? 65 : 35;
        } else if (looking || hiding) {
            m_purple.faceOffX.target = 55;
            m_purple.faceOffY.target = 65;
        } else {
            double cx = m_purple.left + m_purple.width / 2.0;
            double r = mx - cx;
            m_purple.faceOffX.target = qBound(-15.0, r / 20.0, 15.0);
            m_purple.faceOffY.target = qBound(-10.0, (my - BASE_H / 3.0) / 30.0, 10.0);
        }

        double ph = m_purple.heightAnim.val;
        double faceX = m_purple.left + m_purple.translateX.val + m_purple.faceLeft + m_purple.faceOffX.val;
        double faceY = (BASE_H - ph) + m_purple.faceTop + m_purple.faceOffY.val;
        double e1cx = faceX + m_purple.eyeSize / 2.0;
        double e1cy = faceY + m_purple.eyeSize / 2.0;
        double e2cx = faceX + m_purple.eyeSize + m_purple.eyeGap + m_purple.eyeSize / 2.0;
        double e2cy = faceY + m_purple.eyeSize / 2.0;

        double forcePX = -999, forcePY = -999;
        if (showing) { forcePX = m_peeking ? 4 : -4; forcePY = m_peeking ? 5 : -4; }
        else if (looking || hiding) { forcePX = 3; forcePY = 4; }

        auto [p1x, p1y] = calcPupilOffset(e1cx, e1cy, m_purple.maxPupilDist, mx, my, forcePX, forcePY);
        auto [p2x, p2y] = calcPupilOffset(e2cx, e2cy, m_purple.maxPupilDist, mx, my, forcePX, forcePY);
        m_purple.pupil1X.target = p1x; m_purple.pupil1Y.target = p1y;
        m_purple.pupil2X.target = p2x; m_purple.pupil2Y.target = p2y;
    }

    // ===== 深灰角色 =====
    {
        if (showing) {
            m_black.skewX.target = 0;
            m_black.translateX.target = 0;
        } else if (looking) {
            double cx = m_black.left + m_black.width / 2.0;
            double r = mx - cx;
            double baseSkew = qBound(-6.0, -r / 120.0, 6.0);
            m_black.skewX.target = 1.5 * baseSkew + 10;
            m_black.translateX.target = 20;
        } else if (m_typing || hiding) {
            double cx = m_black.left + m_black.width / 2.0;
            double r = mx - cx;
            double baseSkew = qBound(-6.0, -r / 120.0, 6.0);
            m_black.skewX.target = 1.5 * baseSkew;
            m_black.translateX.target = 0;
        } else {
            double cx = m_black.left + m_black.width / 2.0;
            double r = mx - cx;
            m_black.skewX.target = qBound(-6.0, -r / 120.0, 6.0);
            m_black.translateX.target = 0;
        }

        m_black.heightAnim.target = m_black.height;

        if (showing) {
            m_black.faceOffX.target = 10;
            m_black.faceOffY.target = 28;
        } else if (looking) {
            m_black.faceOffX.target = 32;
            m_black.faceOffY.target = 12;
        } else {
            double cx = m_black.left + m_black.width / 2.0;
            double r = mx - cx;
            m_black.faceOffX.target = qBound(-15.0, r / 20.0, 15.0);
            m_black.faceOffY.target = qBound(-10.0, (my - BASE_H / 3.0) / 30.0, 10.0);
        }

        double ph = m_black.heightAnim.val;
        double faceX = m_black.left + m_black.translateX.val + m_black.faceLeft + m_black.faceOffX.val;
        double faceY = (BASE_H - ph) + m_black.faceTop + m_black.faceOffY.val;
        double e1cx = faceX + m_black.eyeSize / 2.0;
        double e1cy = faceY + m_black.eyeSize / 2.0;
        double e2cx = faceX + m_black.eyeSize + m_black.eyeGap + m_black.eyeSize / 2.0;
        double e2cy = faceY + m_black.eyeSize / 2.0;

        double forcePX = -999, forcePY = -999;
        if (showing) { forcePX = -4; forcePY = -4; }
        else if (looking) { forcePX = 0; forcePY = -4; }

        auto [p1x, p1y] = calcPupilOffset(e1cx, e1cy, m_black.maxPupilDist, mx, my, forcePX, forcePY);
        auto [p2x, p2y] = calcPupilOffset(e2cx, e2cy, m_black.maxPupilDist, mx, my, forcePX, forcePY);
        m_black.pupil1X.target = p1x; m_black.pupil1Y.target = p1y;
        m_black.pupil2X.target = p2x; m_black.pupil2Y.target = p2y;
    }

    // ===== 橙色角色 =====
    {
        if (showing) {
            m_orange.skewX.target = 0;
        } else {
            double cx = m_orange.left + m_orange.width / 2.0;
            double r = mx - cx;
            m_orange.skewX.target = qBound(-6.0, -r / 120.0, 6.0);
        }

        m_orange.heightAnim.target = m_orange.height;

        if (showing) {
            m_orange.faceOffX.target = 50;
            m_orange.faceOffY.target = 85;
        } else {
            double cx = m_orange.left + m_orange.width / 2.0;
            double r = mx - cx;
            m_orange.faceOffX.target = r / 20.0;
            m_orange.faceOffY.target = (my - BASE_H / 3.0) / 30.0;
        }

        double ph = m_orange.heightAnim.val;
        double faceX = m_orange.left + m_orange.faceLeft + m_orange.faceOffX.val;
        double faceY = (BASE_H - ph) + m_orange.faceTop + m_orange.faceOffY.val;
        double e1cx = faceX + m_orange.pupilSize / 2.0;
        double e1cy = faceY + m_orange.pupilSize / 2.0;
        double e2cx = faceX + m_orange.pupilSize + m_orange.eyeGap + m_orange.pupilSize / 2.0;
        double e2cy = faceY + m_orange.pupilSize / 2.0;

        double forcePX = -999, forcePY = -999;
        if (showing) { forcePX = -5; forcePY = -4; }

        auto [p1x, p1y] = calcPupilOffset(e1cx, e1cy, m_orange.maxPupilDist, mx, my, forcePX, forcePY);
        auto [p2x, p2y] = calcPupilOffset(e2cx, e2cy, m_orange.maxPupilDist, mx, my, forcePX, forcePY);
        m_orange.pupil1X.target = p1x; m_orange.pupil1Y.target = p1y;
        m_orange.pupil2X.target = p2x; m_orange.pupil2Y.target = p2y;
    }

    // ===== 黄色角色 =====
    {
        if (showing) {
            m_yellow.skewX.target = 0;
        } else {
            double cx = m_yellow.left + m_yellow.width / 2.0;
            double r = mx - cx;
            m_yellow.skewX.target = qBound(-6.0, -r / 120.0, 6.0);
        }

        m_yellow.heightAnim.target = m_yellow.height;

        if (showing) {
            m_yellow.faceOffX.target = 20;
            m_yellow.faceOffY.target = 35;
        } else {
            double cx = m_yellow.left + m_yellow.width / 2.0;
            double r = mx - cx;
            m_yellow.faceOffX.target = r / 20.0;
            m_yellow.faceOffY.target = (my - BASE_H / 3.0) / 30.0;
        }

        double ph = m_yellow.heightAnim.val;
        double faceX = m_yellow.left + m_yellow.faceLeft + m_yellow.faceOffX.val;
        double faceY = (BASE_H - ph) + m_yellow.faceTop + m_yellow.faceOffY.val;
        double e1cx = faceX + m_yellow.pupilSize / 2.0;
        double e1cy = faceY + m_yellow.pupilSize / 2.0;
        double e2cx = faceX + m_yellow.pupilSize + m_yellow.eyeGap + m_yellow.pupilSize / 2.0;
        double e2cy = faceY + m_yellow.pupilSize / 2.0;

        double forcePX = -999, forcePY = -999;
        if (showing) { forcePX = -5; forcePY = -4; }

        auto [p1x, p1y] = calcPupilOffset(e1cx, e1cy, m_yellow.maxPupilDist, mx, my, forcePX, forcePY);
        auto [p2x, p2y] = calcPupilOffset(e2cx, e2cy, m_yellow.maxPupilDist, mx, my, forcePX, forcePY);
        m_yellow.pupil1X.target = p1x; m_yellow.pupil1Y.target = p1y;
        m_yellow.pupil2X.target = p2x; m_yellow.pupil2Y.target = p2y;
    }

    // ---- 插值更新 ----
    auto lerp = [](Character &c) {
        c.skewX.update();
        c.translateX.update();
        c.heightAnim.update();
        c.faceOffX.speed = c.faceSpeed;
        c.faceOffY.speed = c.faceSpeed;
        c.faceOffX.update();
        c.faceOffY.update();
        c.pupil1X.speed = 0.15; c.pupil1Y.speed = 0.15;
        c.pupil2X.speed = 0.15; c.pupil2Y.speed = 0.15;
        c.pupil1X.update(); c.pupil1Y.update();
        c.pupil2X.update(); c.pupil2Y.update();
    };
    lerp(m_purple); lerp(m_black); lerp(m_orange); lerp(m_yellow);

    update();
}

// ==================== 瞳孔计算 ====================

QPair<double,double> AnimatedCharacters::calcPupilOffset(double ecx, double ecy,
                                                           double maxDist, double mx, double my,
                                                           double forceX, double forceY) const
{
    if (forceX != -999 && forceY != -999) return { forceX, forceY };

    double r = mx - ecx;
    double i = my - ecy;
    double dist = qMin(qSqrt(r * r + i * i), maxDist);
    double ang = qAtan2(i, r);
    return { qCos(ang) * dist, qSin(ang) * dist };
}

// ==================== 眨眼 ====================

void AnimatedCharacters::scheduleBlink(Character &c)
{
    int delay = QRandomGenerator::global()->bounded(4000) + 3000;
    QTimer::singleShot(delay, this, [&c, this]() {
        c.blinkProgress = 0.11;
        QTimer::singleShot(150, this, [&c, this]() {
            c.blinkProgress = 1.0;
            scheduleBlink(c);
        });
    });
}

// ==================== 偷瞄 ====================

void AnimatedCharacters::schedulePeek()
{
    int delay = QRandomGenerator::global()->bounded(3000) + 2000;
    m_peekTimer.singleShot(delay, this, [this]() {
        if (!isShowing()) return;
        m_peeking = true;
        QTimer::singleShot(800, this, [this]() { m_peeking = false; });
    });
}

// ==================== 绘制 ====================

void AnimatedCharacters::paintEvent(QPaintEvent *)
{
    int w = width(), h = height();
    if (w <= 0 || h <= 0) return;

    // 缓冲区 = widget实际大小 × 2 (2x抗锯齿)
    int bw = w * SCALE;
    int bh = h * SCALE;
    if (m_buffer.width() != bw || m_buffer.height() != bh) {
        m_buffer = QPixmap(bw, bh);
    }
    m_buffer.fill(Qt::transparent);

    QPainter buf(&m_buffer);
    buf.setRenderHint(QPainter::Antialiasing, true);

    // 第1层缩放: 2x 抗锯齿
    buf.scale(SCALE, SCALE);
    // 现在坐标系 = widget 坐标 (0,0)~(w,h)

    // 第2层缩放: BASE坐标 → widget坐标
    double sx = double(w) / BASE_W;
    double sy = double(h) / BASE_H;
    buf.scale(sx, sy);
    // 现在坐标系 = BASE坐标 (0,0)~(450,500)
    // BASE(0,0) = widget(0,0), BASE(450,500) = widget(w,h)

    // ---- 装饰 ----
    QRadialGradient g1(BASE_W * 0.75, BASE_H * 0.25, 150);
    g1.setColorAt(0, QColor(255, 255, 255, 25));
    g1.setColorAt(1, QColor(255, 255, 255, 0));
    buf.setBrush(QBrush(g1));
    buf.setPen(Qt::NoPen);
    buf.drawEllipse(QPointF(BASE_W * 0.75, BASE_H * 0.25), 150, 150);

    QRadialGradient g2(BASE_W * 0.25, BASE_H * 0.75, 200);
    g2.setColorAt(0, QColor(255, 255, 255, 12));
    g2.setColorAt(1, QColor(255, 255, 255, 0));
    buf.setBrush(QBrush(g2));
    buf.drawEllipse(QPointF(BASE_W * 0.25, BASE_H * 0.75), 200, 200);

    buf.setPen(QPen(QColor(255, 255, 255, 12), 1));
    for (double x = 0; x <= BASE_W; x += 20) buf.drawLine(QPointF(x, 0), QPointF(x, BASE_H));
    for (double y = 0; y <= BASE_H; y += 20) buf.drawLine(QPointF(0, y), QPointF(BASE_W, y));

    // ---- 角色按z-index绘制 ----
    paintCharacter(buf, m_purple);
    paintCharacter(buf, m_black);
    paintCharacter(buf, m_orange);
    paintCharacter(buf, m_yellow);

    buf.end();

    // 1:1 绘制到 widget，零偏移
    QPainter p(this);
    p.drawPixmap(0, 0, m_buffer);
}

void AnimatedCharacters::paintCharacter(QPainter &p, const Character &c)
{
    p.save();

    double cw = c.width;
    double ch = c.heightAnim.val;
    double cx = c.left + c.translateX.val;
    double cy = BASE_H - ch; // 底部对齐 (BASE_H=500 是画布底部)

    // 倾斜: 围绕底部中心 (transform-origin: bottom center)
    double orgX = cx + cw / 2.0;
    double orgY = BASE_H;
    QTransform t;
    t.translate(orgX, orgY);
    if (qAbs(double(c.skewX)) > 0.001) {
        t.shear(qTan(qDegreesToRadians(double(c.skewX))), 0);
    }
    t.translate(-orgX, -orgY);
    p.setTransform(t);

    // ---- 身体 ----
    QPainterPath body;
    double r = qMin(c.radius, cw / 2.0);
    body.moveTo(cx + r, cy);
    body.lineTo(cx + cw - r, cy);
    body.quadTo(cx + cw, cy, cx + cw, cy + r);
    body.lineTo(cx + cw, BASE_H);
    body.lineTo(cx, BASE_H);
    body.lineTo(cx, cy + r);
    body.quadTo(cx, cy, cx + r, cy);
    body.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(c.color);
    p.drawPath(body);

    // ---- 面部 ----
    double faceX = cx + c.faceLeft + c.faceOffX.val;
    double faceY = cy + c.faceTop + c.faceOffY.val;

    if (c.hasEyeball) {
        double es = c.eyeSize;
        double pr = c.pupilSize / 2.0;

        double eyeH = c.blinkProgress < 0.5
            ? 2.0 + (es - 2.0) * (c.blinkProgress * 2.0)
            : es;

        double e1cx = faceX + es / 2.0;
        double e1cy = faceY + es / 2.0;
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(e1cx - es / 2.0, e1cy - eyeH / 2.0, es, eyeH));

        double e2cx = faceX + es + c.eyeGap + es / 2.0;
        double e2cy = faceY + es / 2.0;
        p.drawEllipse(QRectF(e2cx - es / 2.0, e2cy - eyeH / 2.0, es, eyeH));

        if (c.blinkProgress >= 0.5) {
            p.setBrush(QColor("#2D2D2D"));
            p.drawEllipse(QPointF(e1cx + double(c.pupil1X), e1cy + double(c.pupil1Y)), pr, pr);
            p.drawEllipse(QPointF(e2cx + double(c.pupil2X), e2cy + double(c.pupil2Y)), pr, pr);
        }
    } else {
        double ps = c.pupilSize;
        double pr = ps / 2.0;

        double e1cx = faceX + pr;
        double e1cy = faceY + pr;
        double e2cx = faceX + ps + c.eyeGap + pr;
        double e2cy = faceY + pr;

        p.setBrush(QColor("#2D2D2D"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(e1cx + double(c.pupil1X), e1cy + double(c.pupil1Y)), pr, pr);
        p.drawEllipse(QPointF(e2cx + double(c.pupil2X), e2cy + double(c.pupil2Y)), pr, pr);
    }

    // ---- 嘴巴 (黄色) ----
    if (c.hasMouth) {
        double mmx = cx + c.mouthLeft + c.faceOffX.val;
        double mmy = cy + c.mouthTop + c.faceOffY.val;
        p.setBrush(QColor("#2D2D2D"));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(mmx, mmy, c.mouthWidth, 4), 2, 2);
    }

    p.restore();
}
