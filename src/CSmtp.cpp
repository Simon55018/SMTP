#include "CSmtp_p.h"

#define SERVER_READY_CODE       "220"       // 服务就绪代码
#define REQ_COMPLETED_CODE      "250"       // 请求动作成功完成代码
#define DURCH_CODE              "235"       // 认证通过代码
#define PRCOCEEDING_CODE        "221"       // 处理中代码
#define WAIT_CLI_ENTER_CODE     "334"       // 等待客户端输入代码,用于等待用户名,密码输入
#define TRANS_START_CODE        "354"       // 发送开始,与data指令结合
#define ERROR_COMMAND_CODE      "500"       // 指令错误代码
#define CMD_CANNT_EXEC_CODE     "550"       // 命令无法执行代码

#define STRING_READY_REPLY      SERVER_READY_CODE
// 与服务器确认，通知其客户端使用的机器名称，一般邮件服务器不做限定
#define STRING_HELO             QString("helo %1\r\n").arg(m_sHostName.remove("smtp."))
#define STRING_HELO_REPLY       REQ_COMPLETED_CODE
// 使用AUTH LOGIN与服务器进行登录验证
#define STRING_AUTH             QString("auth login\r\n")
#define STRING_AUTH_REPLY       WAIT_CLI_ENTER_CODE
// 发送端邮箱用户名
#define STRING_USER             QString("\r\n").prepend(m_sSenderMail.toLatin1().toBase64())
#define STRING_USER_REPLY       WAIT_CLI_ENTER_CODE
// 发送端邮箱密码
#define STRING_PASSWORD         QString("\r\n").prepend(m_sAuthCode.toLatin1().toBase64())
#define STRING_PASSWORD_REPLY   DURCH_CODE
// 发件人信息，填写与认证信息不同往往被定位为垃圾邮件或恶意邮件
#define STRING_MAIL_FROM        QString("mail from: <%1>\r\n").arg(m_sSenderMail)
#define STRING_MAIL_FROM_REPLY  REQ_COMPLETED_CODE
// 收信人地址
#define STRING_RCPT_TO          QString("rcpt to: <%1>\r\n").arg(m_sReceiveMail)
#define STRING_RCPT_TO_REPLY    REQ_COMPLETED_CODE
// 邮件发送开启指令
#define STRING_DATA             "data\r\n"
#define STRING_DATA_REPLY       TRANS_START_CODE
// 邮件主题内容
#define STRING_CONTENT          QString("from:%1\r\n" \
                                "to:%2\r\n" \
                                "subject:=?UTF-8?B?%3?=\r\n" \
                                "\r\n" \
                                "%4\r\n" \
                                "\r\n" \
                                ".\r\n").arg(m_sSenderMail).arg(m_sReceiveMail) \
                                .arg( QString("").append(m_sTitle.toUtf8().toBase64()) ).arg(m_sContent)
#define STRING_CONTENT_REPLY    REQ_COMPLETED_CODE


CSmtp::CSmtp(QObject *parent)
    : QObject(parent)
{
    Q_D(CSmtp);
    d_ptr->q_ptr = this;

    d->m_socket = NULL;
    d->m_pNextAction = NULL;
    d->m_bReadySend = false;
}

CSmtp::CSmtp(QString sHostName, int lPort, QObject *parent)
    : QObject(parent)
{
    Q_D(CSmtp);

    d_ptr->q_ptr = this;

    d->m_socket = NULL;
    d->m_pNextAction = NULL;
    d->m_bReadySend = false;

    connectToHost(sHostName, lPort);
}

CSmtp::CSmtp(QString sHostName, int lPort, QString sSenderMail,
             QString sAuthCode, QObject *parent)
    : QObject(parent), d_ptr(new CSmtpPrivate)
{
    Q_D(CSmtp);

    d_ptr->q_ptr = this;

    d->m_socket = NULL;
    d->m_pNextAction = NULL;
    d->m_bReadySend = false;

    connectToHost(sHostName, lPort);
    setSenderInfo(sSenderMail, sAuthCode);
}

CSmtp::~CSmtp()
{
    Q_D(CSmtp);
    if( NULL != d->m_socket )
    {
        delete d->m_socket;
        d->m_socket = NULL;
    }

    d->m_pNextAction = NULL;
}

bool CSmtp::connectToHost(QString sHostName, int lPort)
{
    Q_D(CSmtp);

    if( NULL == d->m_socket )
    {
        d->m_socket = new QTcpSocket;
        connect(d->m_socket, SIGNAL(readyRead()), d, SLOT(stReadyRead()));
    }

    d->m_sExpectedReply = STRING_READY_REPLY;
    d->m_pNextAction = &CSmtpPrivate::setReadySend;

    d->m_socket->connectToHost(sHostName, lPort);           // 连接到服务器
    bool bRet = d->m_socket->waitForConnected(1000);        // 等待1s,无法连接则认为断连
    bRet &= d->m_socket->waitForReadyRead(1000);            // 等待SERVER_READY回复,超过一秒认为已经断连

    if( !bRet )
    {
        d->m_sExpectedReply.clear();
        d->m_socket->disconnectFromHost();
    }

    d->m_sHostName = sHostName;
    d->m_lPort = lPort;

    return bRet;
}

bool CSmtp::isConnected()
{
    Q_D(CSmtp);

    if( QAbstractSocket::ConnectedState == d->m_socket->state() )
    {
        return true;
    }

    return false;
}

void CSmtp::setSenderInfo(QString sSenderMail, QString sAuthCode)
{
    Q_D(CSmtp);

    d->m_sSenderMail = sSenderMail;
    d->m_sAuthCode = sAuthCode;
}

bool CSmtp::sendMail(QString sReceiveMail, QString sContent, QString sTitle)
{
    Q_D(CSmtp);
    d->m_sReceiveMail = sReceiveMail;
    d->m_sContent = sContent;
    d->m_sTitle = sTitle;

    emit d->sgStartSend();  // 发送开启发送信号

    return d->exec();       // 进入发送邮件时间循环
}

CSmtpPrivate::CSmtpPrivate()
{
    q_ptr = NULL;

    m_sExpectedReply.clear();
    m_pNextAction = NULL;

    connect(this, SIGNAL(sgStartSend()), this, SLOT(stStartSend()));
}

CSmtpPrivate::~CSmtpPrivate()
{

}

void CSmtpPrivate::stReadyRead()
{
    // 服务端每次回复均为/r/n结尾,所以用readLine,不容易读取无效信息
    QByteArray buffer = m_socket->readLine();
    // 判断接受的buff是否是预想的回复代码
    if( buffer.contains(m_sExpectedReply.toLatin1()) )
    {
        // 为预想代码,则判断下一步的函数指针是否为空,不为空则继续执行
        if( NULL != m_pNextAction )
        {
            (this->*m_pNextAction)();
        }
        else
        {
            exit(false);
        }
    }
    // 回复代码不为预想的回复代码,则退出事件循环,并返回错误
    else
    {
        exit(false);
    }
}

void CSmtpPrivate::stStartSend()
{
    // 开启发送邮件第一步:检查连接状态
    this->checkConnectState();
}

void CSmtpPrivate::checkConnectState()
{
    // 先判断是否处理连接状态
    if( m_bReadySend &&
            QAbstractSocket::ConnectedState == m_socket->state() )
    {
        // 开启发送HELO指令
        this->sendHelo();
    }
    // 不为连接状态则退出时间循环,返回错误
    else
    {
        exit(false);
    }
}

void CSmtpPrivate::sendHelo()
{
    QString str = STRING_HELO;
    m_socket->write(str.toLatin1());
    m_sExpectedReply = STRING_HELO_REPLY;
    m_pNextAction = &CSmtpPrivate::sendAuthLogin;   // 设置下一步为登录验证
}

void CSmtpPrivate::sendAuthLogin()
{
    QString str = STRING_AUTH;
    m_socket->write(str.toLatin1());
    m_sExpectedReply = STRING_AUTH_REPLY;
    m_pNextAction = &CSmtpPrivate::sendUser;    // 设置下一步为发送邮箱用户名
}

void CSmtpPrivate::sendUser()
{
    QString str = STRING_USER;
    m_socket->write(str.toLatin1());
    m_sExpectedReply = STRING_USER_REPLY;
    m_pNextAction = &CSmtpPrivate::sendPassword;    // 设置下一步为发送邮箱密码
}

void CSmtpPrivate::sendPassword()
{
    QString str = STRING_PASSWORD;
    m_socket->write(str.toLatin1());
    m_sExpectedReply = STRING_PASSWORD_REPLY;
    m_pNextAction = &CSmtpPrivate::sendMailFrom;    // 设置下一步为发送发件人邮箱
}

void CSmtpPrivate::sendMailFrom()
{
    QString str = STRING_MAIL_FROM;
    m_socket->write(str.toLatin1());
    m_sExpectedReply = STRING_MAIL_FROM_REPLY;
    m_pNextAction = &CSmtpPrivate::sendRcptTo;      // 设置下一步为发送收件人邮箱
}

void CSmtpPrivate::sendRcptTo()
{
    QString str = STRING_RCPT_TO;
    m_socket->write(str.toLatin1());
    m_sExpectedReply = STRING_RCPT_TO_REPLY;
    m_pNextAction = &CSmtpPrivate::sendData;        // 设置下一部为邮件发送开启指令
}

void CSmtpPrivate::sendData()
{
    QString str = STRING_DATA;
    m_socket->write(str.toLatin1());
    m_sExpectedReply = STRING_DATA_REPLY;
    m_pNextAction = &CSmtpPrivate::sendContent;     // 设置下一步为发送邮件内容
}

void CSmtpPrivate::sendContent()
{
    QString str = STRING_CONTENT;
    m_socket->write(str.toUtf8());
    m_sExpectedReply = STRING_CONTENT_REPLY;
    m_pNextAction = &CSmtpPrivate::successfulSend;  // 设置下一步为邮件发送成功
}

void CSmtpPrivate::successfulSend()
{
    exit(true);
}

void CSmtpPrivate::setReadySend()
{
    m_bReadySend = true;
}
