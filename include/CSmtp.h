#ifndef CSMTP_H
#define CSMTP_H

#include <QEventLoop>
#include <QObject>
#include <QScopedPointer>

class CSmtpPrivate;
class CSmtp : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(CSmtp)
    Q_DISABLE_COPY(CSmtp)
public:
    explicit CSmtp(QObject *parent = 0);

    // 这两个构造均已经自动执行服务器连接
    CSmtp(QString sHostName, int lPort, QObject *parent = 0);
    CSmtp(QString sHostName, int lPort,
          QString sSenderMail, QString sAuthCode, QObject *parent = 0);

    virtual ~CSmtp();

    /*!
     * \brief connectToHost 连接邮箱smtp服务器
     * \param sHostName     [in]    smtp服务器地址,如:QQ邮箱(smtp.qq.com),163邮箱(smtp.163.com)
     * \param lPort         [in]    端口号,默认均为25
     * \return 成功/失败
     */
    bool connectToHost(QString sHostName, int lPort = 25);

    /*!
     * \brief isConnected   判断是否连接到smtp服务器
     * \return 成功/失败
     */
    bool isConnected();

    /*!
     * \brief setSenderInfo 设置发送者信息
     * \param sSenderMail   [in]    邮箱名
     * \param sAuthCode     [in]    邮箱密码
     */
    void setSenderInfo(QString sSenderMail, QString sAuthCode);

    /*!
     * \brief sendMail      发送邮件(为阻塞型函数)
     * \param sReceiveMail  [in]    收件人邮箱
     * \param sContent      [in]    邮件内容(尽量不要重复发送相同的内容,容易被认为垃圾邮件
     * \param sTitle        [in]    邮件主题(可选),但是不填写容易被认为垃圾邮件
     * \return
     */
    bool sendMail(QString sReceiveMail, QString sContent, QString sTitle = 0);

private:
    QScopedPointer<CSmtpPrivate>    d_ptr;
};

#endif // CSMTP_H
