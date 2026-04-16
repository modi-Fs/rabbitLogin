#ifndef INTROOVERLAY_H
#define INTROOVERLAY_H

#include <QWidget>
#include <QBasicTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

class IntroOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit IntroOverlay(QWidget *parent = nullptr);

    void startAnimation();

signals:
    void animationFinished();

protected:
    void timerEvent(QTimerEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // 缓动函数
    static double easeOutCubic(double t);
    static double easeOutBack(double t);
    static double easeInOutCubic(double t);

    // 绘制部分线段
    static QPointF lerpPoint(const QPointF &a, const QPointF &b, double t);

    // 绘制菱形描边动画
    void drawDiamondOutline(QPainter &p, const QPolygonF &diamond,
                            double progress, double offsetX, double offsetY, double scale);

    // 绘制菱形填充动画
    void drawDiamondFill(QPainter &p, const QPolygonF &diamond,
                         double opacity, double offsetX, double offsetY, double scale,
                         const QColor &color);

    QBasicTimer m_timer;
    int m_frame = 0;

    // 动画时长 (帧数, ~60fps)
    static constexpr int TOTAL_FRAMES = 150; // ~2.5秒

    // 菱形顶点 (基于 viewBox 0 0 28 28, 中心在 14,14)
    // 菱形1: M7,14 L12,9 L17,14 L12,19
    QPolygonF m_diamond1;
    // 菱形2: M13,14 L18,9 L21,12 L21,16 L18,19
    QPolygonF m_diamond2;

    // 2x 抗锯齿缓冲区
    static constexpr int SCALE = 2;
    QPixmap m_buffer;
};

#endif // INTROOVERLAY_H
