#ifndef ANIMATEDCHARACTERS_H
#define ANIMATEDCHARACTERS_H

#include <QWidget>
#include <QTimer>
#include <QColor>
#include <QPointF>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QSize>

// 原版基准尺寸 (与原版容器 w-[450px] h-[500px] 一致)
static constexpr double BASE_W = 450.0;
static constexpr double BASE_H = 500.0;

// 原版参数对照:
// 紫色: left:70, w:180, h:400(打字时440), color:#6C3FF5, radius:10px
//        眼: size:18, pupilSize:7, maxDist:5, gap:32(8*4 tailwind gap-8)
//        face: left:45, top:40
//        打字: skewX-12, translateX:40, height+40
// 深灰: left:240, w:120, h:310, color:#2D2D2D, radius:8px
//        眼: size:16, pupilSize:6, maxDist:4, gap:24(6*4 tailwind gap-6)
//        face: left:26, top:32
//        打字: skewX*1.5+10, translateX:20
// 橙色: left:0, w:240, h:200, color:#FF9B6B, radius:120px
//        瞳孔: size:12, maxDist:5, gap:32
//        face: left:82, top:90
// 黄色: left:310, w:140, h:230, color:#E8D754, radius:70px
//        瞳孔: size:12, maxDist:5, gap:24
//        face: left:52, top:40
//        嘴: left:40, top:88, w:80(5rem), h:4px
//
// 交互逻辑:
// bodySkew = clamp(-r/120, -6, 6)
// faceX = clamp(r/20, -15, 15), faceY = clamp(mouseY_diff/30, -10, 10)
// 瞳孔: maxDist clamp, atan2角度计算
// 紫色打字: skewX-12+bodySkew, translateX:40, height:400→440
// 深灰打字: skewX: 1.5*bodySkew+10, translateX:20
// 密码显示时所有skewX=0, 面部偏移到固定位置
// 橙色/黄色面部transition: 200ms (更快), 紫色/深灰: 700ms

struct SmoothVal {
    double val = 0.0;
    double target = 0.0;
    double speed = 0.12;
    void update() { val += (target - val) * speed; }
    operator double() const { return val; }
};

struct Character {
    // 基础属性
    double left = 0;       // CSS left
    double width = 0;      // CSS width
    double height = 0;     // CSS height
    double typingHeight = 0; // 打字时的高度
    QColor color;
    double radius = 0;     // border-radius (top, 对称)
    int zIndex = 0;

    // 眼睛属性
    bool hasEyeball = false; // 有眼白(紫色/深灰=true, 橙色/黄色=false)
    double eyeSize = 0;      // 有眼白时: 眼白直径
    double pupilSize = 0;    // 瞳孔直径
    double maxPupilDist = 0; // 瞳孔最大偏移
    double eyeGap = 0;       // 两眼间距

    // 面部位置 (CSS left/top)
    double faceLeft = 0;
    double faceTop = 0;
    double faceSpeed = 0.12; // 插值速度 (原版: 700ms vs 200ms)

    // 嘴巴 (仅黄色)
    bool hasMouth = false;
    double mouthLeft = 0, mouthTop = 0, mouthWidth = 0;

    // 动画状态
    SmoothVal skewX;       // 身体倾斜度数
    SmoothVal translateX;   // 水平位移
    SmoothVal heightAnim;   // 高度插值
    SmoothVal faceOffX;     // 面部X偏移
    SmoothVal faceOffY;     // 面部Y偏移
    SmoothVal pupil1X, pupil1Y, pupil2X, pupil2Y;
    double blinkProgress = 1.0; // 1=睁开, <1=闭合中
};

class AnimatedCharacters : public QWidget
{
    Q_OBJECT
public:
    explicit AnimatedCharacters(QWidget *parent = nullptr);

    void setTyping(bool typing);
    void setShowPassword(bool show);
    void setPasswordLength(int len);

    QSize sizeHint() const override { return QSize(400, 520); }
    QSize minimumSizeHint() const override { return QSize(400, 400); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void initCharacters();
    void updateAnimation();
    void paintCharacter(QPainter &p, const Character &c);
    void scheduleBlink(Character &c);
    void schedulePeek();

    // 坐标转换: widget坐标 -> 基础坐标系(550x400)
    QPointF widgetToBase(const QPointF &widgetPos) const;

    // 计算身体倾斜 (原版 D函数)
    QPair<double,double> calcBodySkew(const Character &c, double mx, double my,
                                       bool showing, bool looking, bool isTyping, bool hiding) const;

    // 计算瞳孔偏移
    QPair<double,double> calcPupilOffset(double ecx, double ecy,
                                          double maxDist, double mx, double my,
                                          double forceX, double forceY) const;

    Character m_purple, m_black, m_orange, m_yellow;

    // 外部状态
    bool m_typing = false;
    bool m_showPassword = false;
    int m_passwordLength = 0;

    // 内部状态
    bool m_looking = false;  // 打字时紫色/深灰互看
    bool m_peeking = false;  // 密码可见时橙色偷瞄

    bool isHiding() const { return m_passwordLength > 0 && !m_showPassword; }
    bool isShowing() const { return m_passwordLength > 0 && m_showPassword; }

    // 定时器
    QTimer m_animTimer;
    QTimer m_peekTimer;

    // 2x分辨率离屏缓冲区 (抗锯齿)
    static constexpr int SCALE = 2;
    QPixmap m_buffer;
};
#endif
