#ifndef _sinf_interfaces_h_
#define _sinf_interfaces_h_
#endif

#include <stdbool.h>
#include <pthread.h>

//Struct to configure house
struct House {
	char divisoria[8];
	int id;
	char sensores[10][100];
	int n_sensores;
	char atuadores[10][100];
	int n_atuadores;
};

//Struct of Decimal Measurements (obtained in terminal)
struct Measurements{
	int mote_id;
	int id;
	int sensor1;
	int sensor3;
	int sensor4;
	int current;
	int voltage;
};

//Struct with true values for sensors
struct Motes{
	int id;
	float voltage;
	float current;
	float temperature;
	float humidity;
	float light;
	float power;
	int movement;
	float sound;
	float smoke;
};

//Struct to configure actuators
struct Actuators{
	int id;
	char name[20];
	bool heater;
	bool cool;
	bool warnso;
	bool dehum;
	bool warnm;
	bool lamp;
	bool potd;
	bool warsm;
};

//Struct to Rules
struct Rules {
	char name[20];
	int id;
	char cond_sensor[20];
	char sensor[8];
	char operator[3];
	int valors;
	char cond_actuator[20];
	char actuator[8];
	bool on;
};

//Struct to read line from simulator
struct Simulator {
	char linha[256];
	int terminal;
};

pthread_mutex_t mutex_sensors,mutex_room,mutex_actuators;
int state_temp1,state_temp2;

//Number of motes
int nmr_motes(int terminal);

//Value to Real Measurements
float real_voltage (int value,float Vref);
float percentage_smoke (int smoke);
int detect_movement (int value);
float detect_sound (int value);
float real_temperature (int SOt);
float relative_humidity (int SOrh);
float detect_light (int value);
float real_current (int C);
float dissipated_power(float v, float i);

//Find ID
int what_id (char *str);

//Read Config
void* readConfigurations(void* num_divisorias);

//Ler ficheiro das Regras
void* readRules(void* num_rules);

//Read Sensors
void* readSensors(void* arg);

//- True Values for each room
void* true_sensors_values(void* arg);

//Valor absoluto
float valorAbsoluto (float x);

//- CONTROLADOR P
int P_controller (float temperature);

//- Mudar declice do gráfico linear da temperatura, quando o atuador HEATER e COOL atuam
void change_temperature_slope1 (int declive1);
void change_temperature_slope2 (int declive2);

//Verificar regras
void* checkRules(void* num_rules);

//Iniciar Rgb
void init_rgb(void);

//RGB
void* writeRGBMatrix(void* arg);