#include "mbed.h"
#include <stdlib.h>
#include "bg96.h"

using namespace std::chrono;

UnbufferedSerial pc(USBTX, USBRX, 115200);
BufferedSerial modem_int(PA_9, PA_10, 115200);
DigitalOut voltageRegultator(PB_12);
DigitalOut trigger(PB_5);
AnalogIn raindrop(PC_2);
DigitalIn raindropDigInput(PA_8);
int isRaining;
InterruptIn echo(PC_7);
LowPowerTimer timer;
LowPowerTimeout timeOut;
const int kitId = 1;
int distCm = 0;
int totalCm = 0;
int meanCm = 0;
int height;
const int kitHeight = 1000;
unsigned long timeUs = 0;
char *msg;
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

void build_msg_mqtt(int height, unsigned int epoch, int isRaining, int kitId)
{
    printf("\n-----------------------------------------------------------------\n");  
    printf("\nCOMENZANDO PROCESO DE MENSAJE...\n");

    char msg_mqtt[100]; //"{\n\t\"distance\": " + distance_char + ",\n\t\"epoch\": " + epoch_char + "\n}\n";
    sprintf(msg_mqtt, "{\n\t\"height\": %d,\n\t\"epoch\": %u,\n\t\"is_raining\": %d,\n\t\"kitId\": %d\n}\n", height, epoch, isRaining, kitId);

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
    height = kitHeight - meanCm;
    time_t seconds = time(NULL);
    build_msg_mqtt(height, (unsigned int) seconds, isRaining, kitId);
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
}

void start()
{
    trigger.write(1);
    ThisThread::sleep_for(0.01);
    trigger.write(0);
}

void processMeassures()
{
    if(started) {
        timer.stop();
        timeUs = duration_cast<microseconds>(timer.elapsed_time()).count();
        timer.reset();
        distCm = (timeUs*343)/20000; //disminuir error decimal
        started = false;
    }
}

void checkRain()
{
    if(raindropDigInput.read()) {
        isRaining = 0;
    }
    else {
        isRaining = 1;
    }
}

void takeAndSendMeassures()
{
    while(1) {
// se toman 3 medidas y se hace el promedio
        printf("ENCENDER SENSOR!\n");
        totalCm = 0;
        for(int i=0; i<3; i++) {
            start();
            ThisThread::sleep_for(5s);
            totalCm = totalCm + distCm;
        }
        meanCm = totalCm/3;
        if(distCm == 0){
            printf("\nPRIMERA LECTURA\n");
        } else {
            checkRain();
            sendData();
        }
    }
}

int main()
{  
    set_time(1605614108);
    voltageRegultator.write(1);
    printf("LECTURA DE MEDIDAS\n------------------------------\n------------------------------\n");
    trigger.write(1);
    ThisThread::sleep_for(0.002);
    trigger.write(0);
    eventThread.start(callback(&queue, &EventQueue::dispatch_forever));
    echo.rise(&startTimer);
    echo.fall(&processMeassures);
    initServer();
    takeAndSendMeassures();
}
