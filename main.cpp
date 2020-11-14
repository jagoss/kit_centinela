#include "mbed.h"
#include <stdlib.h>
#include "bg96.h"

using namespace std::chrono;

UnbufferedSerial pc(USBTX, USBRX, 115200);
BufferedSerial modem_int(PA_9, PA_10, 115200);
DigitalOut voltageRegultator(PB_12);
DigitalOut trigger(PB_5);
AnalogIn raindrop(PC_2);
DigitalIn isRaining(PA_8);
InterruptIn echo(PC_7);
LowPowerTimer timer;
LowPowerTimeout timeOut;
int kitId = 1;
int distCm = 0;
unsigned long timeUs = 0;
char *msg;
const char *ALERT_MSG = "RAINING";
Thread eventThread;
EventQueue queue;
bool started = false;

//Defino la interfaz UART "pc" como salida estándar
FileHandle *mbed::mbed_override_console(int fd)
{
    return &pc;
}

//Creo objeto bg96 (Pin Tx, Pin Rx, Pin Regulador, Pin PowKey)
bg96 bg96(PA_9, PA_10, PB_6, PB_15);

//Defino buffer para la recepción de mensajes por mqtt
char msg_rx[120];

//char msg_mqtt[] = "Hola mundo desde el Kit_IoT!!!!!";
char test_apn[] = "testnbiot";              //APN MQTT
char oper[] = "74801";                     //Usuario MQTT
char mqtt_user[] = "\"kitcentinela\"";            //Usuario MQTT
char mqtt_pass[] = "\"k1tcent1nela\"";            //Usuario MQTT
char topic_sub[] = "\"kitiot/centinela_datos\"";     //Topic MQTT para subscripción
char topic_pub[] = "\"kitiot/centinela_datos\"";

void initServer()
{
    //Configuración de parámetros MQTT
    bg96.setAPN(test_apn);
    bg96.setOPER(oper);
    bg96.setMqttUser(mqtt_user);
    bg96.setMqttPass(mqtt_pass);
    bg96.setPubMqttTopic(topic_pub);
    bg96.setSubMqttTopic(topic_sub);

    printf("COMENZANDO PROCESO DE CONFIGURACION...\n");

    bg96.turn_on(); //Enciendo modem BG96

    ThisThread::sleep_for(2s);

    //Función de configuración inicial del modem (ver wiki NB-IoT Básico)
    bg96.configInit();

    //Obtengo hora UTC de la red celular y la cargo en el RTC del Core
    bg96.setRTCTime();

    //Función de configuración de parámetros mqtt (ver wiki NB-IoT Básico)
    bg96.mqttConfig();

    printf("FIN PROCESO DE CONFIGURACION!\n");
}

void build_msg_mqtt(int height, unsigned int epoch, int kitId)
{
    printf("\n-----------------------------------------------------------------\n");  
    printf("\nCOMENZANDO PROCESO DE MENSAJE...\n");

    char msg_mqtt[100]; //"{\n\t\"distance\": " + distance_char + ",\n\t\"epoch\": " + epoch_char + "\n}\n";
    sprintf(msg_mqtt, "{\n\t\"height\": %d,\n\t\"epoch\": %u,\n\t\"kitId\": %d\n}\n", height, epoch, kitId);

    printf("\nMENSAJE GENERADO!\n\n");

    //Publico mensaje mqtt
    //Parámetros: Mensaje, QoS, Retain, MsgID
    bg96.sendMqttMsg(msg_mqtt, '0', '0', 0);

    printf("\nMENSAJE ENVIADO CON EXITO!\n\n");
    printf("-----------------------------------------------------------------\n");
}

void printTimeDist(unsigned long timeUs, int dist) {
    printf("Tiempo total: %luus\n", timeUs);
    printf("Distancia: %icm\n", dist);
    printf("------------------------------\n\n");
}

void sendData()
{
    time_t seconds = time(NULL);
    build_msg_mqtt(distCm, (unsigned int) seconds, kitId);
}

void sendAlert() {
    // send ALERT_MSG via mqtt
}

void messageStartTimer()
{
    printf("START TIMER!\n");
}

void messageStopTimer()
{
    printf("STOP TIMER!\n");
}

void startTimer() {
    started = true;
    timer.stop();
    timer.reset();
    timer.start();
    // queue.call(&messageStartTimer);
}

void start()
{
    trigger.write(1);
    ThisThread::sleep_for(0.01);
    trigger.write(0);
    // if (isRaining.read() == 1) {
    //     sendAlert();
    // }
}

void processMeassures()
{
    if(started) {
        timer.stop();
        // queue.call(&messageStopTimer);
        // endTime = timer.elapsed_time();
        timeUs = duration_cast<microseconds>(timer.elapsed_time()).count();
        timer.reset();
        distCm = (timeUs*343)/20000; //disminuir error decimal
        // queue.call(&printTimeDist, timeUs, distCm);
        started = false;
        // queue.call(&sendData, distCm);
        // timeOut.attach(&start, 5s);
    }
}

int main()
{  
    set_time(1256729737);
    voltageRegultator.write(1);
    printf("LECTURA DE MEDIDAS\n------------------------------\n");
    // trigger.write(1);
    // ThisThread::sleep_for(0.002);
    // trigger.write(0);
    // eventThread.start(callback(&queue, &EventQueue::dispatch_forever));
    echo.rise(&startTimer);
    echo.fall(&processMeassures);
    initServer();
    while(1) {
        printf("ENCENDER SENSOR!\n");
        trigger.write(1);
        ThisThread::sleep_for(0.01);
        trigger.write(0);
        ThisThread::sleep_for(15s);
        sendData();
    }
    // timeOut.attach(&start, 5s);
    // build_msg_mqtt(topic_public, height, epoch, kitId);
}
