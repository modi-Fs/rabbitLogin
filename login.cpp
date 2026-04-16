#include "login.h"
#include "ui_login.h"

#include <QAction>
#include <QLineEdit>
#include <QCursor>
#include <QStyle>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QRegularExpression>
#include <QTime>
#include <QSettings>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <qt_windows.h>
#endif

login::login(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::login)
{
    ui->setupUi(this);

    // 初始化注册表
    m_settings = new QSettings(R"(HKEY_CURRENT_USER\Software\Rab_Param)", QSettings::NativeFormat, this);

    setupFrameless();
    setupInputIcons();
    connectSignals();
    centerWindow();

    // 标题栏双击最大化
    ui->titleBar->installEventFilter(this);

    // 加载持久化设置 (主题 + 记住密码)
    QTimer::singleShot(0, this, &login::loadSettings);
}

login::~login()
{
    delete ui;
}

// ==================== 无边框窗口 ====================

void login::setupFrameless()
{
    // 无边框 + 系统菜单 + 窗口置顶按钮
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    // Windows: 启用 DWM 阴影
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
#endif
}

bool login::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST) {
            *result = HTCLIENT;
            QPoint pos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));

            if (ui->titleBar) {
                QPoint titlePos = ui->titleBar->mapFromGlobal(pos);
                if (ui->titleBar->rect().contains(titlePos)) {
                    for (auto *btn : {ui->minBtn, ui->maxBtn, ui->closeBtn, ui->themeToggleBtn}) {
                        if (btn->isVisible() && btn->rect().contains(btn->mapFromGlobal(pos))) {
                            return false;
                        }
                    }
                    *result = HTCAPTION;
                    return true;
                }
            }
            return false;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void login::onMinimize()
{
    showMinimized();
}

void login::onMaximize()
{
    if (m_isMaximized) {
        showNormal();
        m_isMaximized = false;
    } else {
        showMaximized();
        m_isMaximized = true;
    }
}

void login::onClose()
{
    close();
}

bool login::eventFilter(QObject *watched, QEvent *event)
{
    // 标题栏双击最大化
    if (watched == ui->titleBar && event->type() == QEvent::MouseButtonDblClick) {
        onMaximize();
        return true;
    }
    // 关闭按钮 hover 切换图标
    if (watched == ui->closeBtn) {
        if (event->type() == QEvent::Enter) {
            ui->closeBtn->setIcon(QIcon(m_isDarkTheme ? ":/img/close_hover.svg" : ":/img/close_hover.svg"));
        } else if (event->type() == QEvent::Leave) {
            ui->closeBtn->setIcon(QIcon(m_isDarkTheme ? ":/img/close_dark.svg" : ":/img/close.svg"));
        }
    }
    // 最小化按钮 hover 切换图标
    if (watched == ui->minBtn) {
        if (event->type() == QEvent::Enter) {
            ui->minBtn->setIcon(QIcon(m_isDarkTheme ? ":/img/minimize_dark_hover.svg" : ":/img/minimize_hover.svg"));
        } else if (event->type() == QEvent::Leave) {
            ui->minBtn->setIcon(QIcon(m_isDarkTheme ? ":/img/minimize_dark.svg" : ":/img/minimize.svg"));
        }
    }
    // 最大化按钮 hover 切换图标
    if (watched == ui->maxBtn) {
        if (event->type() == QEvent::Enter) {
            ui->maxBtn->setIcon(QIcon(m_isDarkTheme ? ":/img/maximize_dark_hover.svg" : ":/img/maximize_hover.svg"));
        } else if (event->type() == QEvent::Leave) {
            ui->maxBtn->setIcon(QIcon(m_isDarkTheme ? ":/img/maximize_dark.svg" : ":/img/maximize.svg"));
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// ==================== 窗口事件 ====================

void login::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    centerWindow();
}

void login::centerWindow()
{
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect screenGeo = screen->availableGeometry();
        int x = (screenGeo.width()  - width())  / 2;
        int y = (screenGeo.height() - height()) / 2;
        move(x, y);
    }
}


// ==================== 页面切换 ====================

void login::switchPage(Page page)
{
    ui->stackedWidget->setCurrentIndex(page);

    // 切换页面时清除所有错误/成功提示
    hideError(ui->errorLabel);
    hideError(ui->regErrorLabel);
    hideError(ui->forgotErrorLabel);
    hideError(ui->forgotSuccessLabel);

    // 切换页面时通知动画角色停止打字
    ui->animatedCharacters->setTyping(false);
    // 同步角色密码可见状态与当前页面的小眼睛状态
    if (page == LoginPage) {
        ui->animatedCharacters->setShowPassword(m_passwordVisible);
        ui->animatedCharacters->setPasswordLength(ui->passwordEdit->text().length());
    } else if (page == RegisterPage) {
        bool regPwdVisible = (ui->regPasswordEdit->echoMode() == QLineEdit::Normal);
        ui->animatedCharacters->setShowPassword(regPwdVisible);
        ui->animatedCharacters->setPasswordLength(ui->regPasswordEdit->text().length());
    } else {
        ui->animatedCharacters->setShowPassword(false);
        ui->animatedCharacters->setPasswordLength(0);
    }
}

// ==================== 初始化 ====================

void login::setupInputIcons()
{
    // 登录页 - 账号
    QAction *userAction = ui->usernameEdit->addAction(
        QIcon(":/img/user.svg"), QLineEdit::LeadingPosition);
    userAction->setEnabled(false);

    // 登录页 - 密码
    QAction *lockAction = ui->passwordEdit->addAction(
        QIcon(":/img/lock.svg"), QLineEdit::LeadingPosition);
    lockAction->setEnabled(false);

    m_eyeAction = ui->passwordEdit->addAction(
        QIcon(":/img/eye_closed.svg"), QLineEdit::TrailingPosition);

    // 注册页 - 账号
    QAction *regUserAction = ui->regUsernameEdit->addAction(
        QIcon(":/img/user.svg"), QLineEdit::LeadingPosition);
    regUserAction->setEnabled(false);

    // 注册页 - 密码
    QAction *regLockAction = ui->regPasswordEdit->addAction(
        QIcon(":/img/lock.svg"), QLineEdit::LeadingPosition);
    regLockAction->setEnabled(false);

    m_regEyeAction = ui->regPasswordEdit->addAction(
        QIcon(":/img/eye_closed.svg"), QLineEdit::TrailingPosition);

    // 注册页 - 卡密
    QAction *cardAction = ui->regCardKeyEdit->addAction(
        QIcon(":/img/lock.svg"), QLineEdit::LeadingPosition);
    cardAction->setEnabled(false);

    // 注册页 - 密保
    QAction *secAction = ui->regSecurityEdit->addAction(
        QIcon(":/img/user.svg"), QLineEdit::LeadingPosition);
    secAction->setEnabled(false);

    // 忘记密码页 - 账号
    QAction *forgotUserAction = ui->forgotUsernameEdit->addAction(
        QIcon(":/img/user.svg"), QLineEdit::LeadingPosition);
    forgotUserAction->setEnabled(false);

    // 忘记密码页 - 密保
    QAction *forgotSecAction = ui->forgotSecurityEdit->addAction(
        QIcon(":/img/user.svg"), QLineEdit::LeadingPosition);
    forgotSecAction->setEnabled(false);

    // 忘记密码页 - 新密码
    QAction *forgotNewPwdAction = ui->forgotNewPwdEdit->addAction(
        QIcon(":/img/lock.svg"), QLineEdit::LeadingPosition);
    forgotNewPwdAction->setEnabled(false);

    // 忘记密码页 - 确认密码
    QAction *forgotConfirmAction = ui->forgotConfirmPwdEdit->addAction(
        QIcon(":/img/lock.svg"), QLineEdit::LeadingPosition);
    forgotConfirmAction->setEnabled(false);
}

void login::connectSignals()
{
    // ==================== 标题栏按钮 ====================
    connect(ui->minBtn, &QPushButton::clicked, this, &login::onMinimize);
    connect(ui->maxBtn, &QPushButton::clicked, this, &login::onMaximize);
    connect(ui->closeBtn, &QPushButton::clicked, this, &login::onClose);
    ui->closeBtn->installEventFilter(this);
    ui->minBtn->installEventFilter(this);
    ui->maxBtn->installEventFilter(this);

    // 主题切换
    connect(ui->themeToggleBtn, &QPushButton::clicked, this, &login::onThemeToggled);

    // 标题栏双击最大化 (通过 eventFilter 实现)

    // ==================== 登录页信号 ====================

    // 输入变化 -> 通知动画角色
    connect(ui->usernameEdit, &QLineEdit::textChanged, this, [this]() {
        if (ui->usernameEdit->hasFocus()) {
            ui->animatedCharacters->setTyping(true);
        }
    });
    connect(ui->usernameEdit, &QLineEdit::editingFinished, this, [this]() {
        QTimer::singleShot(100, this, [this]() {
            if (!ui->usernameEdit->hasFocus() && !ui->passwordEdit->hasFocus()) {
                ui->animatedCharacters->setTyping(false);
            }
        });
    });
    connect(ui->passwordEdit, &QLineEdit::textChanged,
            this, &login::onPasswordTextChanged);
    connect(ui->passwordEdit, &QLineEdit::editingFinished, this, [this]() {
        QTimer::singleShot(100, this, [this]() {
            if (!ui->passwordEdit->hasFocus() && !ui->usernameEdit->hasFocus()) {
                ui->animatedCharacters->setTyping(false);
            }
        });
    });

    // 密码可见性切换
    connect(m_eyeAction, &QAction::triggered,
            this, &login::togglePasswordVisibility);

    // 登录按钮
    connect(ui->loginBtn, &QPushButton::clicked,
            this, &login::onLoginClicked);

    // 注册/忘记密码链接
    connect(ui->registerLink, &QPushButton::clicked,
            this, &login::onRegisterClicked);
    connect(ui->forgotLink, &QPushButton::clicked,
            this, &login::onForgotClicked);

    // 回车键
    connect(ui->passwordEdit, &QLineEdit::returnPressed,
            ui->loginBtn, &QPushButton::click);
    connect(ui->usernameEdit, &QLineEdit::returnPressed,
            ui->passwordEdit, [this]() { ui->passwordEdit->setFocus(); });

    // 输入时隐藏错误
    connect(ui->usernameEdit, &QLineEdit::textChanged, this, [this]() { hideError(ui->errorLabel); });
    connect(ui->passwordEdit, &QLineEdit::textChanged, this, [this]() { hideError(ui->errorLabel); });

    // ==================== 注册页信号 ====================

    connect(ui->regBackBtn, &QPushButton::clicked, this, [this]() { switchPage(LoginPage); });
    connect(ui->registerBtn, &QPushButton::clicked, this, &login::onRegisterClicked);
    connect(m_regEyeAction, &QAction::triggered, this, &login::toggleRegPasswordVisibility);

    // 注册页输入时隐藏错误 + 通知角色
    connect(ui->regUsernameEdit, &QLineEdit::textChanged, this, [this]() { hideError(ui->regErrorLabel); });
    connect(ui->regPasswordEdit, &QLineEdit::textChanged, this, [this]() {
        hideError(ui->regErrorLabel);
        ui->animatedCharacters->setPasswordLength(ui->regPasswordEdit->text().length());
        if (ui->regPasswordEdit->hasFocus()) {
            ui->animatedCharacters->setTyping(true);
        }
    });
    connect(ui->regCardKeyEdit, &QLineEdit::textChanged, this, [this]() { hideError(ui->regErrorLabel); });
    connect(ui->regSecurityEdit, &QLineEdit::textChanged, this, [this]() { hideError(ui->regErrorLabel); });

    // 注册页输入 -> 通知动画
    for (auto *edit : {ui->regUsernameEdit, ui->regPasswordEdit, ui->regCardKeyEdit, ui->regSecurityEdit}) {
        connect(edit, &QLineEdit::textChanged, this, [this, edit]() {
            if (edit->hasFocus()) ui->animatedCharacters->setTyping(true);
        });
        connect(edit, &QLineEdit::editingFinished, this, [this]() {
            QTimer::singleShot(100, this, [this]() {
                bool anyFocus = ui->regUsernameEdit->hasFocus() ||
                                ui->regPasswordEdit->hasFocus() ||
                                ui->regCardKeyEdit->hasFocus() ||
                                ui->regSecurityEdit->hasFocus();
                if (!anyFocus) ui->animatedCharacters->setTyping(false);
            });
        });
    }

    // 注册页回车跳转
    connect(ui->regUsernameEdit, &QLineEdit::returnPressed, ui->regPasswordEdit, [this]() { ui->regPasswordEdit->setFocus(); });
    connect(ui->regPasswordEdit, &QLineEdit::returnPressed, ui->regCardKeyEdit, [this]() { ui->regCardKeyEdit->setFocus(); });
    connect(ui->regCardKeyEdit, &QLineEdit::returnPressed, ui->regSecurityEdit, [this]() { ui->regSecurityEdit->setFocus(); });
    connect(ui->regSecurityEdit, &QLineEdit::returnPressed, ui->registerBtn, &QPushButton::click);

    // ==================== 忘记密码页信号 ====================

    connect(ui->forgotBackBtn, &QPushButton::clicked, this, [this]() { switchPage(LoginPage); });
    connect(ui->forgotSubmitBtn, &QPushButton::clicked, this, &login::onForgotSubmitClicked);

    // 忘记密码页输入时隐藏提示
    for (auto *edit : {ui->forgotUsernameEdit, ui->forgotSecurityEdit, ui->forgotNewPwdEdit, ui->forgotConfirmPwdEdit}) {
        connect(edit, &QLineEdit::textChanged, this, [this]() {
            hideError(ui->forgotErrorLabel);
            hideError(ui->forgotSuccessLabel);
        });
    }

    // 忘记密码页输入 -> 通知动画
    for (auto *edit : {ui->forgotUsernameEdit, ui->forgotSecurityEdit, ui->forgotNewPwdEdit, ui->forgotConfirmPwdEdit}) {
        connect(edit, &QLineEdit::textChanged, this, [this, edit]() {
            if (edit->hasFocus()) ui->animatedCharacters->setTyping(true);
        });
        connect(edit, &QLineEdit::editingFinished, this, [this]() {
            QTimer::singleShot(100, this, [this]() {
                bool anyFocus = ui->forgotUsernameEdit->hasFocus() ||
                                ui->forgotSecurityEdit->hasFocus() ||
                                ui->forgotNewPwdEdit->hasFocus() ||
                                ui->forgotConfirmPwdEdit->hasFocus();
                if (!anyFocus) ui->animatedCharacters->setTyping(false);
            });
        });
    }

    // 忘记密码页回车跳转
    connect(ui->forgotUsernameEdit, &QLineEdit::returnPressed, ui->forgotSecurityEdit, [this]() { ui->forgotSecurityEdit->setFocus(); });
    connect(ui->forgotSecurityEdit, &QLineEdit::returnPressed, ui->forgotNewPwdEdit, [this]() { ui->forgotNewPwdEdit->setFocus(); });
    connect(ui->forgotNewPwdEdit, &QLineEdit::returnPressed, ui->forgotConfirmPwdEdit, [this]() { ui->forgotConfirmPwdEdit->setFocus(); });
    connect(ui->forgotConfirmPwdEdit, &QLineEdit::returnPressed, ui->forgotSubmitBtn, &QPushButton::click);
}

// ==================== 槽函数 ====================

void login::onPasswordTextChanged(const QString &text)
{
    ui->animatedCharacters->setPasswordLength(text.length());
    if (ui->passwordEdit->hasFocus()) {
        ui->animatedCharacters->setTyping(true);
    }
}

void login::togglePasswordVisibility()
{
    m_passwordVisible = !m_passwordVisible;
    if (m_passwordVisible) {
        ui->passwordEdit->setEchoMode(QLineEdit::Normal);
        m_eyeAction->setIcon(QIcon(":/img/eye_open.svg"));
    } else {
        ui->passwordEdit->setEchoMode(QLineEdit::Password);
        m_eyeAction->setIcon(QIcon(":/img/eye_closed.svg"));
    }
    ui->animatedCharacters->setShowPassword(m_passwordVisible);
}

void login::toggleRegPasswordVisibility()
{
    // 注册页密码可见性切换
    QLineEdit *edit = ui->regPasswordEdit;
    bool visible = (edit->echoMode() == QLineEdit::Normal);
    edit->setEchoMode(visible ? QLineEdit::Password : QLineEdit::Normal);
    m_regEyeAction->setIcon(QIcon(visible ? ":/img/eye_closed.svg" : ":/img/eye_open.svg"));
    // 同步角色密码可见状态
    ui->animatedCharacters->setShowPassword(!visible);
}

void login::onLoginClicked()
{
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();

    QString err;
    if (username.isEmpty()) { showError(ui->errorLabel, "请输入账号"); ui->usernameEdit->setFocus(); return; }
    if (!validateInput(username, err)) { showError(ui->errorLabel, err); ui->usernameEdit->setFocus(); return; }
    if (password.isEmpty()) { showError(ui->errorLabel, "请输入密码"); ui->passwordEdit->setFocus(); return; }
    if (!validateInput(password, err)) { showError(ui->errorLabel, err); ui->passwordEdit->setFocus(); return; }

    hideError(ui->errorLabel);
    setLoginLoading(true);

    // 保存记住密码
    saveCredentials();

    QTimer::singleShot(1200, this, [this]() {
        setLoginLoading(false);
        // TODO: 替换为实际登录逻辑
    });
}

void login::onRegisterClicked()
{
    // 如果是从 registerBtn 点进来的，执行注册逻辑
    if (sender() == ui->registerBtn) {
        QString username = ui->regUsernameEdit->text().trimmed();
        QString password = ui->regPasswordEdit->text();
        QString cardKey  = ui->regCardKeyEdit->text().trimmed();
        QString security = ui->regSecurityEdit->text().trimmed();

        QString err;
        if (username.isEmpty()) { showError(ui->regErrorLabel, "请输入账号"); ui->regUsernameEdit->setFocus(); return; }
        if (!validateInput(username, err)) { showError(ui->regErrorLabel, "账号: " + err); ui->regUsernameEdit->setFocus(); return; }
        if (password.isEmpty()) { showError(ui->regErrorLabel, "请输入密码"); ui->regPasswordEdit->setFocus(); return; }
        if (!validateInput(password, err)) { showError(ui->regErrorLabel, "密码: " + err); ui->regPasswordEdit->setFocus(); return; }
        if (cardKey.isEmpty()) { showError(ui->regErrorLabel, "请输入时长充值卡密"); ui->regCardKeyEdit->setFocus(); return; }
        // 卡密不限制长度，仅校验字符类型
        static QRegularExpression cardRe("^[a-zA-Z0-9!@#$%^&*()_+\\-=\\[\\]{};':\"\\\\|,.<>/?`~]+$");
        if (!cardRe.match(cardKey).hasMatch()) { showError(ui->regErrorLabel, "卡密: 仅支持字母、数字和特殊字符"); ui->regCardKeyEdit->setFocus(); return; }
        if (security.isEmpty()) { showError(ui->regErrorLabel, "请输入忘记密码密保"); ui->regSecurityEdit->setFocus(); return; }
        if (!validateInput(security, err)) { showError(ui->regErrorLabel, "密保: " + err); ui->regSecurityEdit->setFocus(); return; }

        hideError(ui->regErrorLabel);
        setRegisterLoading(true);

        // 模拟后台注册流程 (2秒)
        QTimer::singleShot(2000, this, [this]() {
            setRegisterLoading(false);
            showSuccess(ui->regErrorLabel, "注册成功！即将返回登录页面...");

            // 清空注册表单
            ui->regUsernameEdit->clear();
            ui->regPasswordEdit->clear();
            ui->regCardKeyEdit->clear();
            ui->regSecurityEdit->clear();
            ui->regPasswordEdit->setEchoMode(QLineEdit::Password);
            m_regEyeAction->setIcon(QIcon(":/img/eye_closed.svg"));

            // 1.5秒后自动跳转回登录页
            QTimer::singleShot(1500, this, [this]() { switchPage(LoginPage); });
        });
        return;
    }

    // 从登录页链接点击 -> 切换到注册页
    switchPage(RegisterPage);
}

void login::onForgotClicked()
{
    // 如果是从 forgotSubmitBtn 点进来的 (不会发生, 但保险起见)
    // 从登录页链接点击 -> 切换到忘记密码页
    switchPage(ForgotPage);
}

void login::onForgotSubmitClicked()
{
    QString username = ui->forgotUsernameEdit->text().trimmed();
    QString security = ui->forgotSecurityEdit->text().trimmed();
    QString newPwd   = ui->forgotNewPwdEdit->text();
    QString confirmPwd = ui->forgotConfirmPwdEdit->text();

    QString err;
    if (username.isEmpty()) { showError(ui->forgotErrorLabel, "请输入账号"); ui->forgotUsernameEdit->setFocus(); return; }
    if (!validateInput(username, err)) { showError(ui->forgotErrorLabel, "账号: " + err); ui->forgotUsernameEdit->setFocus(); return; }
    if (security.isEmpty()) { showError(ui->forgotErrorLabel, "请输入密保"); ui->forgotSecurityEdit->setFocus(); return; }
    if (!validateInput(security, err)) { showError(ui->forgotErrorLabel, "密保: " + err); ui->forgotSecurityEdit->setFocus(); return; }
    if (newPwd.isEmpty()) { showError(ui->forgotErrorLabel, "请输入新密码"); ui->forgotNewPwdEdit->setFocus(); return; }
    if (!validateInput(newPwd, err)) { showError(ui->forgotErrorLabel, "新密码: " + err); ui->forgotNewPwdEdit->setFocus(); return; }
    if (confirmPwd.isEmpty()) { showError(ui->forgotErrorLabel, "请重复输入新密码"); ui->forgotConfirmPwdEdit->setFocus(); return; }
    if (!validateInput(confirmPwd, err)) { showError(ui->forgotErrorLabel, "确认密码: " + err); ui->forgotConfirmPwdEdit->setFocus(); return; }
    if (newPwd != confirmPwd) { showError(ui->forgotErrorLabel, "两次输入的密码不一致"); ui->forgotConfirmPwdEdit->setFocus(); return; }

    hideError(ui->forgotErrorLabel);
    hideError(ui->forgotSuccessLabel);
    setForgotLoading(true);

    // 模拟后台重置密码 (2秒)
    QTimer::singleShot(2000, this, [this]() {
        setForgotLoading(false);

        // 模拟: 随机成功或失败
        bool success = (QTime::currentTime().msec() % 3 != 0); // 约67%成功率

        if (success) {
            showSuccess(ui->forgotSuccessLabel, "密码重置成功！即将返回登录页面...");
            // 清空表单
            ui->forgotUsernameEdit->clear();
            ui->forgotSecurityEdit->clear();
            ui->forgotNewPwdEdit->clear();
            ui->forgotConfirmPwdEdit->clear();
            // 1.5秒后跳转回登录
            QTimer::singleShot(1500, this, [this]() { switchPage(LoginPage); });
        } else {
            showError(ui->forgotErrorLabel, "重置失败：账号不存在或密保答案错误，请重试");
        }
    });
}

// ==================== 输入验证 ====================

bool login::validateInput(const QString &text, QString &errorMsg)
{
    if (text.length() < 4) {
        errorMsg = "长度不能少于 4 个字符";
        return false;
    }
    if (text.length() > 20) {
        errorMsg = "长度不能超过 20 个字符";
        return false;
    }
    // 仅允许字母 + 数字 + 特殊字符 (不允许中文和空格等)
    static QRegularExpression re("^[a-zA-Z0-9!@#$%^&*()_+\\-=\\[\\]{};':\"\\\\|,.<>/?`~]+$");
    if (!re.match(text).hasMatch()) {
        errorMsg = "仅支持字母、数字和特殊字符";
        return false;
    }
    return true;
}

// ==================== 辅助方法 ====================

void login::showError(QLabel *label, const QString &msg)
{
    label->setText(msg);
    if (m_isDarkTheme) {
        label->setStyleSheet(
            "color: #ff6b6b;"
            "background: #301515;"
            "border: 1px solid #502020;"
            "border-radius: 8px;"
            "padding: 10px 14px;"
            "font-size: 13px;"
            "margin-top: 8px;"
        );
    } else {
        label->setStyleSheet(
            "color: #dc2626;"
            "background: #fef2f2;"
            "border: 1px solid #fecaca;"
            "border-radius: 8px;"
            "padding: 10px 14px;"
            "font-size: 13px;"
            "margin-top: 8px;"
        );
    }
    label->setVisible(true);

    auto *effect = new QGraphicsOpacityEffect(label);
    effect->setOpacity(0);
    label->setGraphicsEffect(effect);

    auto *anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void login::hideError(QLabel *label)
{
    if (label) label->setVisible(false);
}

void login::showSuccess(QLabel *label, const QString &msg)
{
    label->setText(msg);
    if (m_isDarkTheme) {
        label->setStyleSheet(
            "color: #4ade80;"
            "background: #152015;"
            "border: 1px solid #205020;"
            "border-radius: 8px;"
            "padding: 10px 14px;"
            "font-size: 13px;"
            "margin-top: 8px;"
        );
    } else {
        label->setStyleSheet(
            "color: #059669;"
            "background: #ecfdf5;"
            "border: 1px solid #a7f3d0;"
            "border-radius: 8px;"
            "padding: 10px 14px;"
            "font-size: 13px;"
            "margin-top: 8px;"
        );
    }
    label->setVisible(true);

    auto *effect = new QGraphicsOpacityEffect(label);
    effect->setOpacity(0);
    label->setGraphicsEffect(effect);

    auto *anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void login::setLoginLoading(bool loading)
{
    ui->loginBtn->setEnabled(!loading);
    ui->loginBtn->setText(loading ? "登录中..." : "登 录");
    ui->usernameEdit->setEnabled(!loading);
    ui->passwordEdit->setEnabled(!loading);
}

void login::setRegisterLoading(bool loading)
{
    ui->registerBtn->setEnabled(!loading);
    ui->registerBtn->setText(loading ? "注册中..." : "注 册");
    ui->regUsernameEdit->setEnabled(!loading);
    ui->regPasswordEdit->setEnabled(!loading);
    ui->regCardKeyEdit->setEnabled(!loading);
    ui->regSecurityEdit->setEnabled(!loading);
}

void login::setForgotLoading(bool loading)
{
    ui->forgotSubmitBtn->setEnabled(!loading);
    ui->forgotSubmitBtn->setText(loading ? "提交中..." : "确 定");
    ui->forgotUsernameEdit->setEnabled(!loading);
    ui->forgotSecurityEdit->setEnabled(!loading);
    ui->forgotNewPwdEdit->setEnabled(!loading);
    ui->forgotConfirmPwdEdit->setEnabled(!loading);
}

// ==================== 主题管理 ====================

void login::onThemeToggled()
{
    m_isDarkTheme = !m_isDarkTheme;
    applyTheme(m_isDarkTheme);
    updateThemeIcon(m_isDarkTheme);

    // 持久化主题选择
    m_settings->setValue("darkTheme", m_isDarkTheme);
}

void login::applyTheme(bool isDark)
{
    // 左面板保持不变, 只修改右面板和标题栏

    QString windowBg = isDark ? "#202020" : "#ffffff";
    QString titleBarBg = isDark ? "#202020" : "#ffffff";
    QString titleBarBorder = isDark ? "#333333" : "#e5e7eb";
    QString rightPanelBg = isDark ? "#202020" : "#ffffff";
    QString titleText = isDark ? "#e0e0e0" : "#0f172a";
    QString labelText = isDark ? "#a0a0a0" : "#374151";
    QString inputBg = isDark ? "#2a2a2a" : "#f8fafc";
    QString inputBorder = isDark ? "#404040" : "#e2e8f0";
    QString inputFocusBg = isDark ? "#333333" : "#ffffff";
    QString inputText = isDark ? "#e0e0e0" : "#111827";
    QString linkColor = isDark ? "#a0a0a0" : "#6b7280";
    QString checkboxColor = isDark ? "#a0a0a0" : "#6b7280";
    QString checkboxBorder = isDark ? "#505050" : "#d1d5db";
    QString checkboxBg = isDark ? "#2a2a2a" : "#ffffff";
    QString toggleHover = isDark ? "#333333" : "#e6e6e6";
    QString errorColor = isDark ? "#ff6b6b" : "#dc2626";
    QString errorBg = isDark ? "#301515" : "#fef2f2";
    QString errorBorder = isDark ? "#502020" : "#fecaca";
    QString successColor = isDark ? "#4ade80" : "#059669";
    QString successBg = isDark ? "#152015" : "#ecfdf5";
    QString successBorder = isDark ? "#205020" : "#a7f3d0";
    QString regTitleText = isDark ? "#e0e0e0" : "#0f172a";
    QString forgotTitleText = isDark ? "#e0e0e0" : "#0f172a";
    QString btnHover = isDark ? "#333333" : "#e6e6e6";

    // 标题栏 (顶部圆角 12px)
    ui->titleBar->setStyleSheet(
        QString("#titleBar { background: %1; border-bottom: 1px solid %2; border-top-left-radius: 12px; border-top-right-radius: 12px; }").arg(titleBarBg, titleBarBorder));
    ui->themeToggleBtn->setStyleSheet(
        QString("#themeToggleBtn { background: transparent; border: none; border-radius: 4px; padding: 6px; }"
                "#themeToggleBtn:hover { background: %1; }").arg(toggleHover));

    // 标题栏按钮 (minBtn, maxBtn, closeBtn)
    QString titleBtnStyle = QString(
        "#minBtn { background: transparent; border: none; border-radius: 4px; padding: 8px 15px; }"
        "#minBtn:hover { background: %1; }"
        "#maxBtn { background: transparent; border: none; border-radius: 4px; padding: 8px 15px; }"
        "#maxBtn:hover { background: %1; }"
        "#closeBtn { background: transparent; border: none; border-radius: 4px; padding: 8px 15px; }"
        "#closeBtn:hover { background: #dc2626; }").arg(btnHover);
    ui->minBtn->setStyleSheet(titleBtnStyle);
    ui->maxBtn->setStyleSheet(titleBtnStyle);
    ui->closeBtn->setStyleSheet(titleBtnStyle);

    // 标题栏按钮图标 - 暗黑模式用白色, 白天用灰色
    if (isDark) {
        ui->minBtn->setIcon(QIcon(":/img/minimize_dark.svg"));
        ui->maxBtn->setIcon(QIcon(":/img/maximize_dark.svg"));
        ui->closeBtn->setIcon(QIcon(":/img/close_dark.svg"));
    } else {
        ui->minBtn->setIcon(QIcon(":/img/minimize.svg"));
        ui->maxBtn->setIcon(QIcon(":/img/maximize.svg"));
        ui->closeBtn->setIcon(QIcon(":/img/close.svg"));
    }

    // 左面板 (左侧圆角 12px)
    ui->leftPanel->setStyleSheet(
        isDark
            ? "#leftPanel { background: #2B2B2B; }"
            : "#leftPanel { background: #BFBAAB; }");

    // Rabbit 品牌名文字颜色
    ui->brandNameLabel->setStyleSheet(
        isDark
            ? "#brandNameLabel { color: #ffffff; font-size: 20px; font-weight: 700; }"
            : "#brandNameLabel { color: #000000; font-size: 20px; font-weight: 700; }");

    // 右面板 (三个页面)
    QString pageBg = QString("#loginPage { background: %1; }").arg(rightPanelBg);
    ui->loginPage->setStyleSheet(pageBg);
    ui->registerPage->setStyleSheet(QString("#registerPage { background: %1; }").arg(rightPanelBg));
    ui->forgotPage->setStyleSheet(QString("#forgotPage { background: %1; }").arg(rightPanelBg));

    // 标题文字
    ui->titleLabel->setStyleSheet(
        QString("#titleLabel { color: %1; font-size: 26px; font-weight: 700; letter-spacing: -0.5px; background: transparent; padding-bottom: 24px; }").arg(titleText));

    // 注册页标题
    ui->regTitleLabel->setStyleSheet(
        QString("#regTitleLabel { color: %1; font-size: 22px; font-weight: 700; background: transparent; }").arg(regTitleText));

    // 忘记密码页标题
    ui->forgotTitleLabel->setStyleSheet(
        QString("#forgotTitleLabel { color: %1; font-size: 22px; font-weight: 700; background: transparent; }").arg(forgotTitleText));

    // 通用标签样式
    QString labelStyle = QString("color: %1; font-size: 14px; font-weight: 500; background: transparent; padding-bottom: 6px;").arg(labelText);
    for (auto *lbl : {ui->usernameLabel, ui->passwordLabel, ui->regUsernameLabel, ui->regPasswordLabel,
                       ui->regCardKeyLabel, ui->regSecurityLabel, ui->forgotUsernameLabel,
                       ui->forgotSecurityLabel, ui->forgotNewPwdLabel, ui->forgotConfirmPwdLabel}) {
        lbl->setStyleSheet(labelStyle);
    }

    // 通用输入框样式
    QString inputStyle = QString(
        "QLineEdit {"
        "    background: %1;"
        "    border: 1px solid %2;"
        "    border-radius: 8px;"
        "    padding: 12px 12px 12px 42px;"
        "    font-size: 14px;"
        "    color: %3;"
        "    selection-background-color: #dbeafe;"
        "}"
        "QLineEdit:hover { border-color: #8B6FCE; }"
        "QLineEdit:focus { border-color: #4F31BC; background: %4; }").arg(inputBg, inputBorder, inputText, inputFocusBg);
    for (auto *edit : {ui->usernameEdit, ui->passwordEdit, ui->regUsernameEdit, ui->regPasswordEdit,
                       ui->regCardKeyEdit, ui->regSecurityEdit, ui->forgotUsernameEdit,
                       ui->forgotSecurityEdit, ui->forgotNewPwdEdit, ui->forgotConfirmPwdEdit}) {
        edit->setStyleSheet(inputStyle);
    }

    // 链接
    QString linkStyle = QString("#registerLink { background: transparent; border: none; outline: none; color: %1; font-size: 13px; padding: 4px 0; }"
                                 "#registerLink:hover { color: #7B5FE0; }").arg(linkColor);
    ui->registerLink->setStyleSheet(linkStyle);

    linkStyle = QString("#forgotLink { background: transparent; border: none; outline: none; color: %1; font-size: 13px; padding: 4px 0; }"
                        "#forgotLink:hover { color: #7B5FE0; }").arg(linkColor);
    ui->forgotLink->setStyleSheet(linkStyle);

    QString backStyle = QString("#regBackBtn { background: transparent; border: none; outline: none; color: %1; font-size: 13px; padding: 0 0 8px 0; text-align: left; }"
                                "#regBackBtn:hover { color: #7B5FE0; }").arg(linkColor);
    ui->regBackBtn->setStyleSheet(backStyle);

    backStyle = QString("#forgotBackBtn { background: transparent; border: none; outline: none; color: %1; font-size: 13px; padding: 0 0 8px 0; text-align: left; }"
                        "#forgotBackBtn:hover { color: #7B5FE0; }").arg(linkColor);
    ui->forgotBackBtn->setStyleSheet(backStyle);

    // 记住密码复选框 (统一白色对号 + 紫色背景)
    QString checkImg = ":/img/check_white.svg";
    ui->rememberPwdCheckBox->setStyleSheet(
        QString("#rememberPwdCheckBox { color: %1; font-size: 13px; spacing: 6px; background: transparent; }"
                "#rememberPwdCheckBox::indicator { width: 16px; height: 16px; border: 2px solid %2; border-radius: 3px; background: %3; }"
                "#rememberPwdCheckBox::indicator:checked { background: #4F31BC; border-color: #4F31BC; image: url(%4); }"
                "#rememberPwdCheckBox::indicator:hover { border-color: #4F31BC; }").arg(checkboxColor, checkboxBorder, checkboxBg, checkImg));

    // 存储暗黑模式样式用于 showError/showSuccess
    m_isDarkTheme = isDark;

    // centralwidget 背景色 + 圆角
    ui->centralwidget->setStyleSheet(
        QString("#centralwidget {"
                "  background: %1;"
                "  border-radius: 12px;"
                "}").arg(windowBg));
}

void login::updateThemeIcon(bool isDark)
{
    if (isDark) {
        ui->themeToggleBtn->setIcon(QIcon(":/img/moon.svg"));
        ui->themeToggleBtn->setToolTip("切换到白天主题");
    } else {
        ui->themeToggleBtn->setIcon(QIcon(":/img/sun.svg"));
        ui->themeToggleBtn->setToolTip("切换到暗黑主题");
    }
}

// ==================== 设置持久化 ====================

void login::loadSettings()
{
    // 加载主题
    m_isDarkTheme = m_settings->value("darkTheme", false).toBool();
    applyTheme(m_isDarkTheme);
    updateThemeIcon(m_isDarkTheme);

    // 加载记住密码
    bool remember = m_settings->value("rememberPassword", false).toBool();
    if (remember) {
        QString username = m_settings->value("username", "").toString();
        QString password = m_settings->value("password", "").toString();
        if (!username.isEmpty()) {
            ui->usernameEdit->setText(username);
            ui->passwordEdit->setText(password);
            ui->rememberPwdCheckBox->setChecked(true);
        }
    }
}

void login::saveCredentials()
{
    if (ui->rememberPwdCheckBox->isChecked()) {
        m_settings->setValue("rememberPassword", true);
        m_settings->setValue("username", ui->usernameEdit->text().trimmed());
        m_settings->setValue("password", ui->passwordEdit->text());
    } else {
        m_settings->setValue("rememberPassword", false);
        m_settings->remove("username");
        m_settings->remove("password");
    }
}

void login::loadCredentials()
{
    // 已在 loadSettings 中实现
}
