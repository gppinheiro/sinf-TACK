//LIBRARIES
#include "sinf_interfaces.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <postgresql/libpq-fe.h>

#define MAX_NAME_SZ 256

//PRINCIPAL
int main() {
	const char *dbconn;
    dbconn = "host = 'db.fe.up.pt' dbname = 'sinf1920a25' user = 'sinf1920a25' password = 'nXjjWays'";

	pthread_t thread_config_file,thread_rules_file,thread_sensors0,thread_sensors1,thread_sensors2,thread_sensors3,thread_true_sensors_values,thread_sensor,thread_check,thread_actuator,thread_RGB;

	int number_rules=0,n_divisorias=0;

	struct Simulator str_terminal0;
	struct Simulator str_terminal1;
	struct Simulator str_terminal2;
	struct Simulator str_terminal3;

	FILE* f_terminal0 = fopen("/tmp/ttyV20","r");
	FILE* f_terminal1 = fopen("/tmp/ttyV22","r");
	FILE* f_terminal2 = fopen("/tmp/ttyV24","r");
	FILE* f_terminal3 = fopen("/tmp/ttyV26","r");

	//Ver conexão se OK ou se falha
	if (!db_connection(dbconn)) { printf("Connection to DB failed: %s", PQerrorMessage(conn)); }
	else {
		//Apagar as tabelas existentes
		drop_tables();
		//Criar novas tabelas
		create_tables();
		//Iniciar maquina de estados
		state_temp1=0;
		state_temp2=0;
		for (int a=0; a<10; a++) {
			for (int b=0; b<8; b++) {
				state_actuator[a][b]=0;
			}
		}
		pthread_mutex_init(&mutex_terminal,NULL);
		//Ler ficheiro da configuração
		pthread_create(&thread_config_file,NULL, readConfigurations, &n_divisorias);
		//Ler ficheiro das regras, só precisamos de ler uma vez
		pthread_create(&thread_rules_file, NULL, readRules, &number_rules);
		//Iniciar RGB, não vale a pena criar thread
		init_rgb();
		//Esperar que acabem
		pthread_join(thread_config_file,NULL);
		pthread_join(thread_rules_file,NULL);

		//Começar o ciclo propriamente dito
		while(1){
			//Enquanto a conexão não volta
			while (!db_connection(dbconn)) { printf("Erro de conexão com a base de dados, aguarde...\n"); }
			//Iniciar Mutexes
			pthread_mutex_init(&mutex_sensors,NULL);
			pthread_mutex_init(&mutex_room,NULL);
			//Valores retirados do simulador
			fgets(str_terminal0.linha, MAX_NAME_SZ, f_terminal0);
			str_terminal0.terminal=0;
			pthread_create(&thread_sensors0,NULL, readSensors, &str_terminal0);

			fgets(str_terminal1.linha, MAX_NAME_SZ, f_terminal1);
			str_terminal1.terminal=1;
			pthread_create(&thread_sensors1,NULL, readSensors, &str_terminal1);
		
			fgets(str_terminal2.linha, MAX_NAME_SZ, f_terminal2);
			str_terminal2.terminal=2;
			pthread_create(&thread_sensors2,NULL, readSensors, &str_terminal2);
			
			fgets(str_terminal3.linha, MAX_NAME_SZ, f_terminal3);
			str_terminal3.terminal=3;
			pthread_create(&thread_sensors3,NULL, readSensors, &str_terminal3);
			
			//Valores verdadeiros de cada divisão, com base no ficheiro Sensor_Configuration
			pthread_create(&thread_true_sensors_values,NULL,true_sensors_values,NULL);

			//Dar update ou insert na base de dados
			pthread_create(&thread_sensor,NULL,update_sensor,NULL);

			//WAIT
			pthread_join(thread_sensors0,NULL);
			pthread_join(thread_sensors1,NULL);
			pthread_join(thread_sensors2,NULL);
			pthread_join(thread_sensors3,NULL);
			pthread_join(thread_true_sensors_values,NULL);
			pthread_join(thread_sensor,NULL);

			pthread_mutex_init(&mutex_actuators,NULL);
			//Comparar regras com os valores verdadeiros
			pthread_create(&thread_check,NULL,checkRules,&number_rules);

			//Dar update ou insert na base de dados
			pthread_create(&thread_actuator,NULL,update_actuator,NULL);

			//Imprimir na RGB
			//pthread_create(&thread_RGB,NULL,writeRGBMatrix,NULL);

			//WAIT
			pthread_join(thread_check,NULL);
			pthread_join(thread_actuator,NULL);
			//pthread_join(thread_RGB,NULL);
		}
	}
	
	//Close Terminal Stream
	fclose(f_terminal1);
	fclose(f_terminal2);
	fclose(f_terminal3);
	fclose(f_terminal0);

	PQfinish(conn);

	return 0;
}
