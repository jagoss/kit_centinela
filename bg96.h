#ifndef BG96_H
#define BG96_H

// #ifndef MBED_H

#include "mbed.h"

// #endif

class bg96
{

    DigitalOut *reg;
    DigitalOut *m_powKey;
    BufferedSerial *modem;

    char rx_buffer[120];

    char APN[50];            //Max length 50
    char OPER[50];           //Max length 50
    char mqtt_topic_pub[50]; //Max length 50
    char mqtt_topic_sub[50]; //Max length 50
    char mqtt_user[50];      //Max length 50
    char mqtt_password[50];  //Max length 50

    enum State
    {
        off,
        rdy,
        no_sim,
        asleep,
        connected,
        mqtt_conn,
        mqtt_rdy,
        mqtt_noconn,
        no_conn,
        sending,
        error
    };

    State state;

    void send_cmd(char *cmd);
    void getResp();
    int checkResp(char *resp);
    int checkIP();
    void checkConn();
    void checkMqttConn();
    void flushRxBuffer();
    void clearRxBuffer();

public:
    bg96(PinName Tx, PinName Rx, PinName regPin, PinName powKeyPin);
    ~bg96();

    void setAPN(const char *newApn);
    void setOPER(const char *newOper);
    void setPubMqttTopic(const char *newMqttTopic);
    void setSubMqttTopic(const char *newMqttTopic);
    void setMqttUser(const char *newUser);
    void setMqttPass(const char *newMqttPass);

        void turn_on();
    void turn_off();
    void sleep();
    void wake_up();

    void checkSim();

    //void configInit(void (*handler)(int resp));
    void configInit();
    void mqttConfig();
    int sendMqttMsg(char *msg, char qos, char retain, int msgID);
    void fetchMqttMsg(char qos, char *msg);
    void setRTCTime();
    int getRssi();

    //int mqtt_send(char msg);
};

#endif