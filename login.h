#ifndef LOGIN_H
#define LOGIN_H

#include <QMainWindow>
#include <QLabel>
#include <QGraphicsOpacityEffect>

QT_BEGIN_NAMESPACE
namespace Ui { class login; }
QT_END_NAMESPACE

class QAction;
class QSettings;

class login : public QMainWindow
{
    Q_OBJECT

public:
    login(QWidget *parent = nullptr);
    ~login();

protected:
    void showEvent(QShowEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onPasswordTextChanged(const QString &text);
    void togglePasswordVisibility();
    void toggleRegPasswordVisibility();
    void onLoginClicked();
    void onRegisterClicked();
    void onForgotClicked();
    void onForgotSubmitClicked();
    void onMinimize();
    void onMaximize();
    void onClose();
    void onThemeToggled();

private:
    void setupFrameless();
    void setupInputIcons();
    void connectSignals();
    void centerWindow();

    // 主题管理
    void applyTheme(bool isDark);
    void updateThemeIcon(bool isDark);

    // 设置持久化
    void loadSettings();
    void saveCredentials();
    void loadCredentials();

    void showError(QLabel *label, const QString &msg);
    void hideError(QLabel *label);
    void showSuccess(QLabel *label, const QString &msg);

    void setLoginLoading(bool loading);
    void setRegisterLoading(bool loading);
    void setForgotLoading(bool loading);

    bool validateInput(const QString &text, QString &errorMsg);

    Ui::login *ui;
    QAction *m_eyeAction = nullptr;
    QAction *m_regEyeAction = nullptr;
    bool m_passwordVisible = false;
    bool m_isMaximized = false;
    bool m_isDarkTheme = false;

    QSettings *m_settings = nullptr;

    enum Page { LoginPage = 0, RegisterPage = 1, ForgotPage = 2 };
    void switchPage(Page page);
};

#endif // LOGIN_H
