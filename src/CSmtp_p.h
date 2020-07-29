#ifndef CSMTP_P_H
#define CSMTP_P_H

#include "CSmtp.h"
#include <QTcpSocket>


class CSmtpPrivate : public QEventLoop
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(CSmtp)

public:
    CSmtpPrivate();
    virtual ~CSmtpPrivate();

    //检查连接状态
    void checkConnectState();
    //发送helo
    void sendHelo();
    //发送auth login
    void sendAuthLogin();
    //发送用户名
    void sendUser();
    //发送密码
    void sendPassword();
    //发送mail from
    void sendMailFrom();
    //发送rcpt to
    void sendRcptTo();
    //发送data
    void sendData();
    //发送邮件内容
    void sendContent();
    //发送成功
    void successfulSend();
    //设置发送准备状态
    void setReadySend();

signals:
    //邮件开始发送指令
    void sgStartSend();

private slots:
    //用于处理socket传输的数据
    void stReadyRead();
    //用户处理邮件开始发送指令
    void stStartSend();

private:
    QTcpSocket      *m_socket;

    bool            m_bReadySend;

    QString         m_sHostName;
    int             m_lPort;
    QString         m_sSenderMail;
    QString         m_sAuthCode;
    QString         m_sReceiveMail;
    QString         m_sContent;
    QString         m_sTitle;
    QString         m_sExpectedReply;                  //期待收到的应答
    void            (CSmtpPrivate::*m_pNextAction)();  //收到正确应答后下一步要执行的方法

    CSmtp   *q_ptr;
};
#endif // CSMTP_P_H
