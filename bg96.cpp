#include "bg96.h"

#define RX_DELAY 700ms
#define RETRY 3 //Cantidad de reintentos en el envío de comandos AT
#define DEBUG 1 //Imprime mensaje de debug en consola

#define PRINTFDEBUG(...)                               \
    {                                                  \
        if (DEBUG)                                     \
        {                                              \
            printf("\n");                              \
            printf("[MODEM_DEBUG]\n");                 \
            printf("%s\n", (const char *)__VA_ARGS__); \
            printf("--------------\n");                \
        }                                              \
    }

bg96::bg96(PinName Tx, PinName Rx, PinName regPin, PinName powKeyPin)
{
    reg = new DigitalOut(regPin);
    m_powKey = new DigitalOut(powKeyPin);
    modem = new BufferedSerial(Tx, Rx, 115200);
    state = off;

    strcpy(APN, "APN\0");                     //Max length 50
    strcpy(OPER, "OPER\0");                   //Max length 50
    strcpy(mqtt_topic_pub, "\"pubTopic\"\0"); //Max length 50
    strcpy(mqtt_topic_sub, "\"subTopic\"\0"); //Max length 50
    strcpy(mqtt_user, "\"user\"\0");          //Max length 50
    strcpy(mqtt_password, "\"pass\"\0");      //Max length 50
}

bg96::~bg96()
{
}

void bg96::send_cmd(char *cmd)
{
    flushRxBuffer();
    if (DEBUG)
    {
        printf("%s", cmd);
    }

    int cmd_size = strlen(cmd);
    modem->write(cmd, cmd_size);
}

void bg96::getResp()
{

    modem->read(rx_buffer, sizeof(rx_buffer));
    printf("%s\n", rx_buffer);
    clearRxBuffer();
}

void bg96::flushRxBuffer()
{
    char a[1];
    while (modem->readable())
    {
        modem->read(a, sizeof(a));
    }
}

int bg96::checkResp(char *resp)
{
    int retry = 1;

    while (retry == 1)
    {
        if (modem->readable())
        {
            retry = 0;
            char resp_ok[] = "OK\0";
            modem->read(rx_buffer, sizeof(rx_buffer));

            PRINTFDEBUG(rx_buffer);

            char *respOK;
            char *respCmd;

            respOK = strstr(rx_buffer, resp_ok);

            if (respOK != NULL)
            {
                respCmd = strstr(rx_buffer, resp);

                if (respCmd != NULL)
                {
                    clearRxBuffer();
                    return 1;
                }
                else
                {
                    clearRxBuffer();
                    return 0;
                };
            }
            else
            {
                clearRxBuffer();
                return 0;
            }
        }
    }

    PRINTFDEBUG("Error in Rx buffer...\n");
    clearRxBuffer();
    return -1;
}

int bg96::checkIP()
{
    int retry = 1;

    while (retry == 1)
    {
        if (modem->readable())
        {
            retry = 0;
            char resp_ok[] = "OK\0";
            char resp_ip[] = "+CGPADDR:\0";

            modem->read(rx_buffer, sizeof(rx_buffer));

            char *respOK;
            char *respIP;

            respOK = strstr(rx_buffer, resp_ok);

            if (respOK != NULL)
            {
                respIP = strstr(rx_buffer, resp_ip);

                if (*(respIP + 12) != '0')
                {
                    clearRxBuffer();
                    return 1;
                }
                else
                {
                    clearRxBuffer();
                    return 0;
                };
            }
            else
            {
                clearRxBuffer();
                return 0;
            }
        }
    }

    PRINTFDEBUG("Error in checkIP...\n");
    clearRxBuffer();
    return -1;
}

void bg96::checkConn()
{
    char cmd_cgpaddr[] = "AT+CGPADDR=1\r\0";

    this->send_cmd(cmd_cgpaddr);
    ThisThread::sleep_for(RX_DELAY);

    if (checkIP())
    {
        this->state = connected;
        PRINTFDEBUG("Connected\n");
    }
    else
    {
        this->state = no_conn;
        PRINTFDEBUG("No Connection...\n");
    }
}

void bg96::checkMqttConn()
{

    char req_qmtconn[] = "AT+QMTCONN?\r\0";
    char resp_qmtconn[] = "+QMTCONN: 0,3\0";

    this->send_cmd(req_qmtconn);
    ThisThread::sleep_for(RX_DELAY);

    if (checkResp(resp_qmtconn))
    {
        this->state = mqtt_conn;
    }
    else
    {
        this->state = mqtt_noconn;
    }
};

void bg96::clearRxBuffer()
{
    memset(rx_buffer, '\0', sizeof(rx_buffer));
}

void bg96::setAPN(const char *newApn)
{
    strcpy(APN, newApn);
}

void bg96::setOPER(const char *newOper)
{
    strcpy(OPER, newOper);
}

void bg96::setPubMqttTopic(const char *newMqttTopic)
{
    strcpy(mqtt_topic_pub, newMqttTopic);
}

void bg96::setSubMqttTopic(const char *newMqttTopic)
{
    strcpy(mqtt_topic_sub, newMqttTopic);
}

void bg96::setMqttUser(const char *newUser)
{
    strcpy(mqtt_user, newUser);
}

void bg96::setMqttPass(const char *newMqttPass)
{
    strcpy(mqtt_password, newMqttPass);
}

void bg96::turn_on()
{
    reg->write(1);
    ThisThread::sleep_for(1s);
    m_powKey->write(1);
    ThisThread::sleep_for(2s);
    m_powKey->write(0);

    ThisThread::sleep_for(5s);
    char bg96_rdy[] = {"\r\nRDY\r\n\r\nAPP RDY\r\n"};

    modem->read(rx_buffer, sizeof(rx_buffer));

    if (strcmp(bg96_rdy, rx_buffer) == 0)
    {
        clearRxBuffer();
        PRINTFDEBUG("BG96 RDY\n");
        this->state = rdy;
    }
    else
    {
        clearRxBuffer();
        PRINTFDEBUG("BG96 !RDY\n");
    };
}

void bg96::turn_off()
{
    reg->write(0);
    this->state = off;
}

void bg96::sleep()
{
    m_powKey->write(1);
    ThisThread::sleep_for(2s);
    m_powKey->write(0);

    ThisThread::sleep_for(2s);

    if (modem->readable())
    {
        modem->read(rx_buffer, sizeof(rx_buffer));

        char bg96_asleep[] = {"\r\nNORMAL POWER DOWN\r\n"};

        if (strcmp(bg96_asleep, rx_buffer) == 0)
        {
            PRINTFDEBUG("BG96 is Zzzz\n");
        }
        else
        {
            PRINTFDEBUG("BG96 still ON\n");
        };

        clearRxBuffer();
    }
    else
    {
        PRINTFDEBUG("No Response... BG96 Connected??\n");
    }
}

void bg96::wake_up()
{
    m_powKey->write(1);
    ThisThread::sleep_for(2s);
    m_powKey->write(0);

    ThisThread::sleep_for(5s);

    if (modem->readable())
    {
        modem->read(rx_buffer, sizeof(rx_buffer));

        char bg96_rdy[] = {"\r\nRDY\r\n\r\nAPP RDY\r\n"};

        if (strcmp(bg96_rdy, rx_buffer) == 0)
        {
            PRINTFDEBUG("BG96 RDY\n");
        }
        else
        {
            PRINTFDEBUG("BG96 !RDY\n");
        };

        clearRxBuffer();
    }
    else
    {
        PRINTFDEBUG("No Response... BG96 Connected??\n");
    }
}

void bg96::checkSim()
{

    char check_sim[] = "AT+CPIN?\r\0";

    this->send_cmd(check_sim);
    ThisThread::sleep_for(1s);

    char sim_rdy[] = "\r\n+CPIN: READY\r\n\r\nOK\r\n";

    if (checkResp(sim_rdy))
    {
        PRINTFDEBUG("SIM Ready\n");
    }
    else
    {
        PRINTFDEBUG("Cmd failed!\n");
    };
}

void bg96::configInit()
{

    int retry = 0;
    int initEnd = 0;

    char cmd_ate[] = "ATE0\r\0";
    char cmd_cereg[] = "AT+CEREG?\r\0";
    char cmd_cfun4[] = "AT+CFUN=4\r\0";
    char cmd_cfun1[] = "AT+CFUN=1\r\0";
    char cmd_qcfg1[] = "AT+QCFG=\"nwscanmode\",3,1\r\0";
    char cmd_qcfg2[] = "AT+QCFG=\"iotopmode\",1,1\r\0";
    char cmd_cgdcont1[] = "AT+CGDCONT= 1,\"IP\",\"\0";
    char cmd_cgdcont2[] = "\",\"0.0.0.0\",0,0\r\0";
    char cmd_cops[] = "AT+COPS=1,2,\"74801\",9\r\0";
    char cmd_cgact[] = "AT+CGACT=1,1\r\0";
    char cmd_cgpaddr[] = "AT+CGPADDR=1\r\0";

    char resp_ok[] = "\r\nOK\r\n\0";
    char resp_cereg[] = "\r\n+CEREG: 0,1\r\n\r\nOK\r\n\0";

    enum InitStep
    {
        step_0 = 0,
        step_1,
        step_2,
        step_3,
        step_4,
        step_5,
        step_6,
        step_7,
        step_8,

    };

    InitStep initStep = step_0;

    this->send_cmd(cmd_ate);
    ThisThread::sleep_for(RX_DELAY);

    checkConn();

    if (this->state != connected) //If no connection, start config commands
    {
        while (!initEnd && retry < 4)
        {
            switch (initStep)
            {

            case step_0:
                this->send_cmd(cmd_cfun4);
                ThisThread::sleep_for(RX_DELAY);

                if (checkResp(resp_ok) > 0)
                {
                    initStep = step_1;
                    PRINTFDEBUG("CFUN=4 OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_1:
                this->send_cmd(cmd_qcfg1);
                ThisThread::sleep_for(RX_DELAY);

                if (checkResp(resp_ok) > 0)
                {
                    initStep = step_2;
                    PRINTFDEBUG("QCFG1 OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_2:
                this->send_cmd(cmd_qcfg2);
                ThisThread::sleep_for(RX_DELAY);

                if (checkResp(resp_ok) > 0)
                {
                    initStep = step_3;
                    PRINTFDEBUG("QCFG2 OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_3:
                this->send_cmd(cmd_cgdcont1);
                this->send_cmd(APN);
                this->send_cmd(cmd_cgdcont2);
                ThisThread::sleep_for(RX_DELAY);

                if (checkResp(resp_ok) > 0)
                {
                    initStep = step_4;
                    PRINTFDEBUG("CGDCONT OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_4:
                this->send_cmd(cmd_cfun1);
                ThisThread::sleep_for(RX_DELAY);

                if (checkResp(resp_ok) > 0)
                {
                    initStep = step_5;
                    PRINTFDEBUG("CFUN=1 OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_5:

                this->send_cmd(cmd_cops);
                ThisThread::sleep_for(5s);

                if (checkResp(resp_ok) > 0)
                {
                    initStep = step_6;
                    PRINTFDEBUG("COPS OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_6:

                this->send_cmd(cmd_cereg);
                ThisThread::sleep_for(RX_DELAY);

                if (checkResp(resp_cereg) > 0)
                {
                    initStep = step_7;
                    PRINTFDEBUG("CEREG OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_7:

                this->send_cmd(cmd_cgact);
                ThisThread::sleep_for(RX_DELAY);

                if (checkResp(resp_ok) > 0)
                {
                    initStep = step_8;
                    PRINTFDEBUG("CGACT OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_8:

                this->send_cmd(cmd_cgpaddr);
                ThisThread::sleep_for(RX_DELAY);

                if (checkIP() > 0)
                {
                    initEnd = 1;
                    PRINTFDEBUG("CGPADDR OK\n");
                    break;
                }
                else
                {
                    retry++;
                    break;
                }
            }
        }

        if (initEnd)
        {
            this->state = connected;
            PRINTFDEBUG("Connected!\n");
        }
        else
        {
            this->state = error;
            PRINTFDEBUG("ERROR\n");
            PRINTFDEBUG("%s%i\n", "ConfigStep= ", initStep);
        }
    }
}

void bg96::mqttConfig()
{
    int retry = 0;
    int configEnd = 0;
    char cmd_qmtopen[] = "AT+QMTOPEN=0,\"kitiot.antel.com.uy\",1883\r\0";
    char cmd_qmtconn1[] = "AT+QMTCONN=0,\"nodo\",\0";
    char cmd_qmtconn2[] = ",\0";
    char cmd_qmtconn3[] = "\r\0";
    char resp_ok[] = "\r\nOK\r\n\0";
    char resp_qmtconn[] = "+QMTCONN: 0,0,0\0";

    enum InitStep
    {
        step_0 = 0,
        step_1,
    };

    InitStep initStep = step_0;

    checkConn(); //Check for valid IP

    if (this->state == connected)
    {
        while (!configEnd && retry < 4)
        {
            switch (initStep)
            {

            case step_0:
                this->send_cmd(cmd_qmtopen);
                ThisThread::sleep_for(7s);

                if (checkResp(resp_ok) > 0)
                {
                    initStep = step_1;
                    PRINTFDEBUG("QMTOPEN OK\n");
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    break;
                }

            case step_1:
                this->send_cmd(cmd_qmtconn1);
                this->send_cmd(mqtt_user);
                this->send_cmd(cmd_qmtconn2);
                this->send_cmd(mqtt_password);
                this->send_cmd(cmd_qmtconn3);

                ThisThread::sleep_for(RX_DELAY + 1s);

                if (checkResp(resp_qmtconn) > 0)
                {
                    PRINTFDEBUG("QMTCONN OK\n");
                    configEnd = 1;
                    retry = 0;
                    break;
                }
                else
                {
                    retry++;
                    if (DEBUG)
                    {
                        printf("%s%i\n", "Retry: ", retry);
                    }

                    break;
                }
            }
        }

        if (configEnd)
        {
            this->state = mqtt_conn;
            PRINTFDEBUG("MQTT RDY!\n");
        }
        else
        {
            this->state = error;
            PRINTFDEBUG("ERROR\n");
            if (DEBUG)
            {
                printf("%s%i\n", "ConfigStep = ", initStep);
            }
        }
    }
};

int bg96::sendMqttMsg(char *msg, char qos, char retain, int msgID)
{

    int retry = 0;
    int pub_rdy = 0;
    char msgID_str[5];

    sprintf(msgID_str, "%i", msgID); //Convert msgID int to string

    char cmd_qmtpub1[] = "AT+QMTPUB=0,\0";
    char cmd_qmtpub2[] = ",0,0,\0";

    *(cmd_qmtpub2 + 1) = qos;
    *(cmd_qmtpub2 + 3) = retain;

    char cmd_qmtpub3[] = "\r\0";
    char cmd_endMsg[] = {0x1A, 0};
    char resp_mqttRdy[] = ">";

    checkMqttConn();

    if (state == mqtt_conn)
    {
        while (!pub_rdy && retry < 4)
        {
            this->send_cmd(cmd_qmtpub1);
            this->send_cmd(msgID_str);
            this->send_cmd(cmd_qmtpub2);
            this->send_cmd(mqtt_topic_pub);
            this->send_cmd(cmd_qmtpub3);

            ThisThread::sleep_for(RX_DELAY + 1s);
            modem->read(rx_buffer, sizeof(rx_buffer));

            PRINTFDEBUG(rx_buffer);

            char *mqttrdy;

            mqttrdy = strstr(rx_buffer, resp_mqttRdy);

            if (*(mqttrdy) == '>')
            {

                PRINTFDEBUG("Rdy to send...\n");
                pub_rdy = 1;
                retry = 0;
            }
            else
            {
                retry++;
            }

            clearRxBuffer();
        }

        if (pub_rdy) //Modem ready to send mqttt msg
        {

            char resp_qmtpub[] = "+QMTPUB\0";

            this->send_cmd(msg);
            this->send_cmd(cmd_endMsg);

            ThisThread::sleep_for(RX_DELAY + 1s);
            modem->read(rx_buffer, sizeof(rx_buffer));

            PRINTFDEBUG(rx_buffer);

            if (strstr(rx_buffer, resp_qmtpub) != NULL)
            {
                if (strstr(rx_buffer, msgID_str) != NULL) //Check MsgID in response
                {
                    PRINTFDEBUG("Message sent!");
                    clearRxBuffer();
                    return 1;
                }
                else
                {
                    PRINTFDEBUG("Error sending msg...");
                    clearRxBuffer();
                    return -1;
                }
            }
            else
            {
                PRINTFDEBUG("Message not sent...");
                clearRxBuffer();
                return -1;
            }
        }
        else
        {
            PRINTFDEBUG("Error in AT+QMTPUB");
            clearRxBuffer();
            return -1;
        }
    }
    else
    {
        PRINTFDEBUG("No MQTT connection established...");
        this->state = error;
        return -1;
    }
};

void bg96::fetchMqttMsg(char qos, char *msg)
{
    char cmd_qmtsub1[] = "AT+QMTSUB=0,1,\0";
    char cmd_qmtsub2[] = ",0\r\0";

    *(cmd_qmtsub2 + 1) = qos;

    char cmd_qmtuns1[] = "AT+QMTUNS=0,1,\0";
    char cmd_qmtuns2[] = "\r\0";

    char resp_qmtsub[] = "QMTSUB: 0,1,0,0\0";
    *(resp_qmtsub + 14) = qos;
    char resp_qmtrecv[] = "+QMTRECV: 0,\0";
    char resp_qmtuns[] = "+QMTUNS: 0,1,0\0";

    checkConn();

    if (this->state == connected)
    {
        checkMqttConn();

        if (this->state == mqtt_conn)
        {

            this->send_cmd(cmd_qmtsub1);
            this->send_cmd(mqtt_topic_sub);
            this->send_cmd(cmd_qmtsub2);

            ThisThread::sleep_for(2s);

            modem->read(rx_buffer, sizeof(rx_buffer));
            PRINTFDEBUG(rx_buffer);

            char *respOK;
            char *respCmd;

            respOK = strstr(rx_buffer, resp_qmtsub);

            if (respOK != NULL)
            {
                PRINTFDEBUG("SUB Succesful!!\n");

                respCmd = strstr(rx_buffer, resp_qmtrecv);

                if (respCmd != NULL)
                {
                    PRINTFDEBUG("Msg Received!!\n");
                    int index = 0;

                    //Copy "MsgID, Topic, Payload" to msg
                    while (*(respCmd + index) != '\0' && index < sizeof(rx_buffer))
                    {
                        *(msg + index) = *(respCmd + 12 + index);
                        index++;
                    }

                    PRINTFDEBUG(msg);

                    clearRxBuffer();
                }
                else
                {
                    clearRxBuffer();
                    PRINTFDEBUG("No message received...")
                };

                //Unsubscribe after msg fetch
                this->send_cmd(cmd_qmtuns1);
                this->send_cmd(mqtt_topic_sub);
                this->send_cmd(cmd_qmtuns2);
                ThisThread::sleep_for(1s);
                PRINTFDEBUG(rx_buffer);

                if (checkResp(resp_qmtuns))
                {
                    PRINTFDEBUG("Unsub succesful!")
                }
                else
                {
                    PRINTFDEBUG("Unsub error")
                    this->state = error;
                }
            }
            else
            {
                PRINTFDEBUG("Sub Error...");
                clearRxBuffer();
                this->state = error;
            }
        }
    }
    else
    {
        PRINTFDEBUG("Conn Error...");
        this->state = error;
    }
};

void bg96::setRTCTime()
{
    struct tm t = {0};
    char cmd_getUTC[] = "AT+CCLK?\r\0";

    char resp_getUTC[] = "+CCLK:\0";
    char resp_ok[] = "OK\0";

    this->send_cmd(cmd_getUTC);
    ThisThread::sleep_for(RX_DELAY);

    modem->read(rx_buffer, sizeof(rx_buffer));

    PRINTFDEBUG(rx_buffer);

    char *respOK;
    char *respCmd;

    respOK = strstr(rx_buffer, resp_ok);

    if (respOK != NULL)
    {
        respCmd = strstr(rx_buffer, resp_getUTC);

        if (respCmd != NULL)
        {

            //+CCLK: "20/10/07,18:50:26-12"
            int year = (*(respCmd + 8) - '0') * 10 + (*(respCmd + 9) - '0');
            int month = (*(respCmd + 11) - '0') * 10 + (*(respCmd + 12) - '0') - 1;
            int day = (*(respCmd + 14) - '0') * 10 + (*(respCmd + 15) - '0');
            int hour = (*(respCmd + 17) - '0') * 10 + (*(respCmd + 18) - '0');
            int min = (*(respCmd + 20) - '0') * 10 + (*(respCmd + 21) - '0');
            int sec = (*(respCmd + 23) - '0') * 10 + (*(respCmd + 24) - '0');

            t.tm_year = year + 100; //year 0 = 1900
            t.tm_mon = month;
            t.tm_mday = day;
            t.tm_hour = hour;
            t.tm_min = min;
            t.tm_sec = sec;

            time_t timeSinceEpoch = mktime(&t);

            if (DEBUG)
            {
                printf("%s%lli\n", "Time since Epoch: ", timeSinceEpoch);
            }

            set_time(mktime(&t));

            time_t seconds = time(NULL);

            char buffer[32];
            strftime(buffer, 32, "%H:%M\n", localtime(&seconds));

            if (DEBUG)
            {
                printf("%s\n", buffer);
            }

            clearRxBuffer();
        }
        else
        {
            clearRxBuffer();
        };
    }
    else
    {
        clearRxBuffer();
    }
}
