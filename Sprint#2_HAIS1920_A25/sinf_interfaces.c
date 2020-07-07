//GABRIEL OUTEIRO, GUILHERME PINHEIRO & TOMAS ARAUJO -> TACK -> SINF -> 2020 2S
//OUR LIBRARY
#include "sinf_interfaces.h"

//LIBRARIES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <postgresql/libpq-fe.h>

//DEFINES
#define MAX_NAME_SZ 256
#define MAX_NAME_BUFFER 320
#define NUMBER_ROOMS 10
#define NUMBER_CELLS 5
#define KP 0.001
#define T_IDEAL 20

//GLOBAL VARIABLES
char *C1, *C2, *C3, *C4, *C5, *C6, *C7, *C8, *C9, *C10, *C11, *C11, *C12, *C13, *C14, *C15, *C16, *C17, *C18, *C19, *C20, *C21, *C22, *C23, *C24, *C25;
int numero_sensores_total, numero_atuadores_total, adicional, slope_heater1, slope_heater2, slope_cool1, slope_cool2;

//STRUCTS
struct House home[NUMBER_ROOMS];
struct Measurements sensors[NUMBER_ROOMS];
struct Motes room[NUMBER_ROOMS];
struct Actuators actuators[NUMBER_ROOMS];
struct Rules *file_rules;

//FILE
FILE *f_msgcreator1,*f_msgcreator2;

//PGresult para cada PQexec
PGresult *res_config,*res_rules,*res_adicional;

//RGB COLOR
#define AMARELO "[254,254,0]"         //LUZ
#define AZUL "[0,0,254]"              //COOL
#define BRANCO "[254,254,254]"        //SEM ATUADOR
#define CASTANHO "[188,143,143]"      //HUMIDADE
#define CINZENTO "[128,128,128]"      //FUMO 
#define LARANJA "[254,164,0]"         //AVISO DE EXCESSO DE POT
#define PRETO "[0,0,0]"               //AVISO DE SOM
#define VERDE "[0,254,0]"             //MOVIMENTO
#define VERMELHO "[254,0,0]"          //HEATER

//FUNCTIONS CREATED BY US

//- CONNECTION
int db_connection (const char *connection) {
	conn = PQconnectdb(connection);
	PQexec(conn,"SET search_path TO tack"); //tack is your schema
	
	if (!conn){
		printf("libpq error: PQconnectdb returned NULL. \n\n");
		return 0;
	}
	
	else if (PQstatus(conn) != CONNECTION_OK){
		return 0;
	}

	else {
        return 1;
	}

    return 0;
}

// DROP TABLE - CASCADE por questões de segurança devido ás foreign keys
void drop_tables(void) {
    PQexec(conn,"DROP TABLE house_configuration CASCADE;");
    PQexec(conn,"DROP TABLE file_rules CASCADE;");
}

// CREATE TABLE - Apenas a house_configuration e a file_rules apagamos, dai fazermos sempre o CREATE TABLE. As outras so cria se ainda nao existirem na base de dados
void create_tables (void) {
    PQexec(conn,"CREATE TABLE house_configuration ( div VARCHAR(10) NOT NULL, div_id INTEGER, sensor1 VARCHAR(10), sensor2 VARCHAR(10), sensor3 VARCHAR(10), sensor4 VARCHAR(10), actuator1 VARCHAR(10), actuator2 VARCHAR(10), actuator3 VARCHAR(10), actuator4 VARCHAR(10), CONSTRAINT unique_id UNIQUE(div_id), PRIMARY KEY(div,div_id) ); ");
    PQexec(conn,"CREATE TABLE file_rules ( file_line INTEGER PRIMARY KEY, div VARCHAR(10), sensor_name VARCHAR(10), operator VARCHAR(3) NOT NULL, value_to_compare INTEGER, actuator_name VARCHAR(10), actuator_state BOOLEAN, start TIME, finish TIME );");        
    PQexec(conn,"CREATE TABLE IF NOT EXISTS house_sensor_historic ( div VARCHAR(10) NOT NULL, div_id INTEGER, sensor1 VARCHAR(10), sensor2 VARCHAR(10), sensor3 VARCHAR(10), sensor4 VARCHAR(10), time TIMESTAMP ); ");
    PQexec(conn,"CREATE TABLE IF NOT EXISTS division ( div VARCHAR(10), div_id INTEGER );");
    PQexec(conn,"CREATE TABLE IF NOT EXISTS sensor ( location VARCHAR(10), sensor VARCHAR(10) PRIMARY KEY , sensor_name VARCHAR(10), sensor_id INTEGER, measure DOUBLE PRECISION );");
    PQexec(conn,"CREATE TABLE IF NOT EXISTS sensor_measurements ( location VARCHAR(10), sensor VARCHAR(10) REFERENCES sensor(sensor), sensor_name VARCHAR(10), sensor_id INTEGER, measure DOUBLE PRECISION, sensor_time TIMESTAMP );");
    PQexec(conn,"CREATE TABLE IF NOT EXISTS actuator ( actuator VARCHAR(10) PRIMARY KEY, actuator_name VARCHAR(10), actuator_id INTEGER, actuator_state BOOLEAN );");
    PQexec(conn,"CREATE TABLE IF NOT EXISTS actuator_state ( actuator VARCHAR(10) REFERENCES actuator(actuator), actuator_name VARCHAR(10), actuator_id INTEGER, actuator_state BOOLEAN, actuator_time TIMESTAMP );");
}

// DELETE IF ADD AND REMOVE ONE DIVISION (just in case)
void delete_adicional(void) {
    pthread_mutex_lock(&mutex_db);
    res_adicional=PQexec(conn,"SELECT * FROM division WHERE div_id=2 OR div_id=4;");
    if(PQntuples(res_adicional)!=0) {
        PQexec(conn,"DELETE FROM division WHERE div_id=2;");
        PQexec(conn,"DELETE FROM division WHERE div_id=4;");
        PQexec(conn,"DELETE FROM sensor WHERE sensor_id=2;");
        PQexec(conn,"DELETE FROM sensor WHERE sensor_id=4;");
        PQexec(conn,"DELETE FROM actuator WHERE actuator_id=2;");
        PQexec(conn,"DELETE FROM actuator WHERE actuator_id=4;");
    }
    pthread_mutex_unlock(&mutex_db);
}

//- HOW MANY MOTES?
int nmr_motes(int terminal) {
    char stringConf[MAX_NAME_SZ],motes[4];

    FILE *fp;
    if(terminal==1) {fp=fopen("MsgCreatorConf1.txt","r");}
    else if (terminal==2) {fp=fopen("MsgCreatorConf2.txt","r");}
    else if (terminal==3) {fp=fopen("MsgCreatorConf3.txt","r");}
    fgets(stringConf,MAX_NAME_SZ,fp);

    int i=0;
    while(stringConf[i] != '\0') {
        if(stringConf[i]=='n') {
            if(stringConf[i+3] == ' ') {
                motes[0]=stringConf[i+2];
                motes[1]='\0';
            }
            else  {
                motes[0]=stringConf[i+2];
                motes[1]=stringConf[i+3];
                motes[2]='\0';
            }
        }
        i++;
    }

    fclose(fp);

    return atoi(motes);
}

//- VOLTAGE, V
float real_voltage (int value,float Vref) {
    float voltage = (float) (value/4096)*Vref ;
    return voltage;
}

//- SMOKE, %
float percentage_smoke (int smoke) {
    float percentagem = (float)(smoke*100/8192);
    return percentagem;
}

//- MOVEMENT %
int detect_movement (int value) {
    if (value>10) { return 100; } 
    else { return 0; }
}

//- SOUND, %
float detect_sound (int value) {
    float percentagem = (float)(value*100/4096);
    return percentagem;
}

//- TEMPERATURE, ºC
float real_temperature (int SOt) {
    float T = (float) -41.6 + 0.0106*SOt ;
    //float T = (float) -39.6 + 0.01*SOt;
    return T;
}

//- RELATIVE HUMIDITY, %
float relative_humidity (int SOrh) {
    float RHlinear= (float) -2.0468 + 0.0367*SOrh + (-1.5955*0.000001*SOrh*SOrh);
    return RHlinear;
}

//- LUZ, %
float detect_light (int value) {
    float percentagem = (float)(value*100/44);
    return percentagem;
} 

//- CURRENT, A
float real_current (int C) {
    float current=(float) 0.769 * 100000 * ((C/4096)*1.5/100000)*1000;
    return current;
}

//- POWER, W
float dissipated_power(float v, float i) {
    return v*i;
}

//- FIND ID
int what_id (char *str) {
    if (!strcmp(str,"ROOM1")) { return 0; }
    else if (!strcmp(str,"ROOM2")) { return 1; }
    else if (!strcmp(str,"ROOM3")) { return 3; }
    else if (!strcmp(str,"WC")) { return 5; }
    else if (!strcmp(str,"OFFI")) { return 6; }
    else if (!strcmp(str,"LROOM")) { return 7; }
    else if (!strcmp(str,"LAUN")) { return 8; }
    else if (!strcmp(str,"KITC")) { return 9; }
    else if (!strcmp(str,"ROOM4")) { return 4; }
    else if (!strcmp(str,"WC2")) { return 2; }
    else { return 10; }
}

//- FIND NAME
char* what_name (int id) {
    if (id==0) { return "ROOM1"; }
    else if (id==1) { return "ROOM2"; }
    else if (id==3) { return "ROOM3"; }
    else if (id==5) { return "WC"; }
    else if (id==6) { return "OFFI"; }
    else if (id==7) { return "LROOM"; }
    else if (id==8) { return "LAUN"; }
    else if (id==9) { return "KITC"; }
    else if (id==4) { return "ROOM4"; }
    else if (id==2) { return "WC2"; }
    else { return "ERROR"; }
}

//- READ CONFIG FILE
void* readConfigurations(void* num_divisorias) {
    //Variaveis a usar nesta função
    int i=0,j=0,w=0;
    char linha[MAX_NAME_SZ], *first_token, *second_token, *third_token, *rest;
    char cmd_sql[MAX_NAME_SZ];

    //Passar argumento void para int, como queremos
    int *n_divisorias = (int*) num_divisorias;

    //Alocar memória
    first_token = (char*)malloc(30*sizeof(char));
    second_token = (char*)malloc(30*sizeof(char));
    third_token = (char*)malloc(30*sizeof(char));

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    //Inicializar contador
    numero_atuadores_total=0;
    numero_sensores_total=0;

    //Abrir ficheiro das configurações
    FILE *f_configuration = fopen("Sensor_Configuration","r");

    while(!feof(f_configuration)) {
        //Lê a linha
        fgets(linha,MAX_NAME_SZ,f_configuration);
        rest = linha;

        //Incrementador a 0, para dividirmos a linha em 3
        i=0;
        while((first_token = strtok_r(rest,":",&rest))) {
            if(i==0){ 
                strcpy(home[*n_divisorias].divisoria,first_token);
                home[*n_divisorias].id=what_id(first_token);
            }
            else if(i==1) {
                //Dividir sensores em cada sensor
                second_token = strtok(first_token,";");
                j=0;
                while(second_token!=NULL) {
                    strcpy(home[*n_divisorias].sensores[j],second_token);
                    numero_sensores_total++;         
                    j++;
                    second_token = strtok(NULL,";");
                }
            } 
            else if(i==2) {
                //Dividir atuadores em cada sensor
                third_token = strtok(first_token,";");
                w=0;
                while(third_token!=NULL) {
                    strcpy(home[*n_divisorias].atuadores[w],third_token);
                    numero_atuadores_total++;         
                    w++;
                    third_token = strtok(NULL,";");
                }
            }               
            i++;
        }
        //Inserir dentro da config_casa - TEST
        pthread_mutex_lock(&mutex_db);
        sprintf(cmd_sql,"INSERT INTO house_configuration (div, div_id, sensor1, sensor2, sensor3, sensor4, actuator1, actuator2, actuator3, actuator4) VALUES ('%s','%d','%s','%s','%s','%s','%s','%s','%s','%s');",home[*n_divisorias].divisoria,home[*n_divisorias].id,home[*n_divisorias].sensores[0],home[*n_divisorias].sensores[1],home[*n_divisorias].sensores[2],home[*n_divisorias].sensores[3],home[*n_divisorias].atuadores[0],home[*n_divisorias].atuadores[1],home[*n_divisorias].atuadores[2],home[*n_divisorias].atuadores[3]);
        PQexec(conn, cmd_sql);
        sprintf(cmd_sql,"INSERT INTO house_sensor_historic (div, div_id, sensor1, sensor2, sensor3, sensor4, time) VALUES ('%s','%d','%s','%s','%s','%s','%d-%d-%d %d:%d:%d');",home[*n_divisorias].divisoria,home[*n_divisorias].id,home[*n_divisorias].sensores[0],home[*n_divisorias].sensores[1],home[*n_divisorias].sensores[2],home[*n_divisorias].sensores[3],tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
        PQexec(conn, cmd_sql);
        sprintf(cmd_sql,"INSERT INTO division (div,div_id) SELECT '%s','%d' WHERE NOT EXISTS (SELECT 1 FROM division WHERE div='%s' AND div_id ='%d');",home[*n_divisorias].divisoria,home[*n_divisorias].id,home[*n_divisorias].divisoria,home[*n_divisorias].id);
        PQexec(conn,cmd_sql);
        pthread_mutex_unlock(&mutex_db);
        //Aumentar mais uma n_divisoria
        (*n_divisorias)++;
    }

    //Fechar a stream do ficheiro
    fclose(f_configuration);

    //Variável adicional para saber quantos quartos a mais foram adicionados
    if((*n_divisorias)>8) { adicional = (*n_divisorias)-8; }
    else { adicional=0; delete_adicional(); }

    pthread_mutex_lock(&mutex_terminal);
    pthread_mutex_lock(&mutex_db);
    PQexec(conn,"ALTER TABLE division ADD FOREIGN KEY(div,div_id) REFERENCES house_configuration(div,div_id);");
    res_config = PQexec(conn, "SELECT * FROM house_configuration");
    int rows_config = PQntuples(res_config);
    printf("house_configuration:\n");
    for(int i=0; i<rows_config; i++) {
        for(int j=0; j<10; j++) { printf("%s ", PQgetvalue(res_config,i,j)); }
        printf("\n");
    } 
    PQclear(res_config);
    pthread_mutex_unlock(&mutex_db);
    pthread_mutex_unlock(&mutex_terminal);

    //Libertar memoria
    free(first_token);
    free(second_token);
    free(third_token);
    
    pthread_exit(NULL);
}

//- READ RULES FILE    
void* readRules(void* num_rules) {
    //Variaveis a usar nesta função
    char *first_token,*second_token,*third_token,*fourth_token,linha[MAX_NAME_SZ],db_linha[MAX_NAME_SZ];
    int i=0,j=0,w=0;
    char cmd_sql[MAX_NAME_SZ];
    
    struct tm inicio;
    struct tm fim;

    //Passar argumento void para int, como queremos
    int *number_rules = (int*) num_rules;

    //Alocar memoria
    first_token = (char*)malloc(30*sizeof(char));
    second_token = (char*)malloc(30*sizeof(char));
    third_token = (char*)malloc(30*sizeof(char));
    fourth_token = (char*)malloc(10*sizeof(char));
    file_rules=(struct Rules *)malloc(55*sizeof(struct Rules));
    
    //Abrir ficheiro das regras
    FILE *f_rules= fopen("Sensor_Rules","r");
    
    //Ler ficheiro completo
    while(fgets(linha,MAX_NAME_SZ,f_rules) != NULL) {
        strcpy(db_linha,linha);
        //Le primeiro elemento
        first_token = strtok(linha,":");
        
        //Incrementador a 0, para dividirmos a linha em 3
        i=0;
        while(first_token!=NULL) {
            if(i==0){ strcpy(file_rules[*number_rules].name,first_token); }
            else if(i==1){strcpy(file_rules[*number_rules].cond_sensor,first_token);} 
            else if(i==2){strcpy(file_rules[*number_rules].cond_actuator,first_token);} 
            else if(i==3){strcpy(file_rules[*number_rules].hours,first_token);}                 
            i++;
            first_token = strtok(NULL,":");
        }
        
        //Transformar name em ID
        file_rules[*number_rules].id = what_id(file_rules[*number_rules].name);
        
        //Dividir a condição do sensor em 3
        second_token = strtok(file_rules[*number_rules].cond_sensor," ");
        j=0;
        while(second_token!=NULL) {
            if(j==0){ strcpy(file_rules[*number_rules].sensor,second_token); }
            else if(j==1){strcpy(file_rules[*number_rules].operator,second_token);} 
            else if(j==2){file_rules[*number_rules].valors=atoi(second_token);}              
            j++;
            second_token = strtok(NULL," ");
        }
        
        //Dividir a condição do atuador em 2
        third_token = strtok(file_rules[*number_rules].cond_actuator,"=");
        w=0;
        while(third_token!=NULL) {
            if(w==0){ strcpy(file_rules[*number_rules].actuator,third_token); }
            else if(w==1){ 
                if( ( !strcmp(third_token,"ON;\n") ) || ( !strcmp(third_token,"ON") ) ) { file_rules[*number_rules].on = true; }
                else if( ( !strcmp(third_token,"OFF;\n") ) || ( !strcmp(third_token,"OFF") ) ) { file_rules[*number_rules].on = false; }
            }
            w++;
            third_token = strtok(NULL,"=");
        }

        //Dividir as horas em horas iniciais e finais
        if ( strcmp(file_rules[*number_rules].hours,"") ){
            fourth_token = strtok(file_rules[*number_rules].hours,"h");
            file_rules[*number_rules].start_hour = atoi(fourth_token);
            
            fourth_token = strtok(NULL," ");
            file_rules[*number_rules].start_min = atoi(fourth_token);
            
            fourth_token = strtok(NULL,"h");
            file_rules[*number_rules].end_hour = atoi(fourth_token);
            
            fourth_token = strtok(NULL,";\n");
            file_rules[*number_rules].end_min = atoi(fourth_token);
            
            inicio.tm_hour=file_rules[*number_rules].start_hour;
            fim.tm_hour=file_rules[*number_rules].end_hour;
            inicio.tm_min=file_rules[*number_rules].start_min;
            fim.tm_min=file_rules[*number_rules].end_min;
        }
        else {
            file_rules[*number_rules].start_hour = -1; 
            file_rules[*number_rules].end_hour= -1; 
            inicio.tm_hour=0;
            fim.tm_hour=0;
            inicio.tm_min=0;
            fim.tm_min=0;
        }

        //INSERIR na tabela das regras
        pthread_mutex_lock(&mutex_db);
        sprintf(cmd_sql,"INSERT INTO file_rules (file_line, div, sensor_name, operator, value_to_compare, actuator_name, actuator_state, start, finish) VALUES('%d','%s','%s','%s','%d','%s','%d','%d:%d','%d:%d');",*number_rules,file_rules[*number_rules].name,file_rules[*number_rules].sensor,file_rules[*number_rules].operator,file_rules[*number_rules].valors,file_rules[*number_rules].actuator,file_rules[*number_rules].on,inicio.tm_hour,inicio.tm_min,fim.tm_hour,fim.tm_min);
        while(!PQisthreadsafe());
        PQexec(conn, cmd_sql);
        pthread_mutex_unlock(&mutex_db);
        
        //Numero de regras aumenta
        (*number_rules)++;
    }

    fclose(f_rules);

    pthread_mutex_lock(&mutex_terminal);
    pthread_mutex_lock(&mutex_db);
    res_rules = PQexec(conn, "SELECT * FROM file_rules");
    int rows_rules = PQntuples(res_rules);
    printf("file_rules:\n");
    for(int i=0; i<rows_rules; i++) {
        for(int j=0; j<9; j++) { printf("%s ", PQgetvalue(res_rules,i,j)); }
        printf("\n");
    } 
    PQclear(res_rules);
    pthread_mutex_unlock(&mutex_db);
    pthread_mutex_unlock(&mutex_terminal);

    free(first_token);
    free(second_token);
    free(third_token);

    pthread_exit(NULL);
}

//- READ SENSORS
void* readSensors(void* arg) {
    //Passar a struct de void para struct propriamente dita
    struct Simulator *str_terminal = (struct Simulator *) arg;
    //Variaveis a usar
    char *mote_id, *voltage, *sensor1, *current, *sensor3, *sensor4,*token;
    int motes_nmr=0,id_da_mote=0,id=0;
    //Alocar memoria
    mote_id = (char*)malloc(6*sizeof(char)+1);
    voltage = (char*)malloc(6*sizeof(char)+1);
    sensor1 = (char*)malloc(6*sizeof(char)+1);
    current = (char*)malloc(6*sizeof(char)+1);
    sensor3 = (char*)malloc(6*sizeof(char)+1);
    sensor4 = (char*)malloc(6*sizeof(char)+1);
    
    if(str_terminal->terminal==1) {motes_nmr=nmr_motes(1);}
    else if (str_terminal->terminal==2) {motes_nmr=nmr_motes(2);}
    else if (str_terminal->terminal==3) {motes_nmr=nmr_motes(3);}
    
    //Read every motes
    for(int i=0; i<motes_nmr; i++) {
        //Read first token
        token= strtok(str_terminal->linha," ");

        //Inicial Position
        int counter_pos=0;
        
        while(token != NULL) {
            
            //MOTE_ID
            if(counter_pos==5) { strcpy(mote_id,token); }
            else if(counter_pos==6) { strcat(mote_id,token); }
            //VOLTAGE
            else if(counter_pos==10) { strcpy(voltage,token); }
            else if(counter_pos==11) { strcat(voltage,token); }
            //SENSOR1
            else if(counter_pos==12) { strcpy(sensor1,token); }
            else if(counter_pos==13) { strcat(sensor1,token); }
            //CURRENT
            else if(counter_pos==14) { strcpy(current,token); }
            else if(counter_pos==15) { strcat(current,token); }
            //SENSOR3   
            else if(counter_pos==16) { strcpy(sensor3,token); }
            else if(counter_pos==17) { strcat(sensor3,token); }
            //SENSOR4
            else if(counter_pos==18) { strcpy(sensor4,token); }
            else if(counter_pos==19) { strcat(sensor4,token); }

            counter_pos++;
            token = strtok(NULL," "); 
        }

        id_da_mote=strtol(mote_id,NULL,10);
        
        if (id_da_mote==0) { id = 0; }
        else if (id_da_mote==1) { id = 1; }
        else if (id_da_mote==2) { id = 3; }
        else if (id_da_mote==3) { id = 6; }
        else if (id_da_mote==4) { id = 7; }
        else if (id_da_mote==5) { id = 5; }
        else if (id_da_mote==6) { id = 8; }
        else if (id_da_mote==7) { id = 9; }
        else if (id_da_mote==8) { id = 2; }
        else if (id_da_mote==9) { id = 4; }
        
        pthread_mutex_lock(&mutex_sensors);
        sensors[id].mote_id = id_da_mote;
        sensors[id].id = id;
        sensors[id].voltage = strtol(voltage,NULL,16);
        sensors[id].sensor1 = strtol(sensor1,NULL,16);
        sensors[id].current = strtol(current,NULL,16);
        sensors[id].sensor3 = strtol(sensor3,NULL,16);
        sensors[id].sensor4 = strtol(sensor4,NULL,16);
        pthread_mutex_unlock(&mutex_sensors);
    }

    free(mote_id);
    free(voltage);
    free(sensor1);
    free(current);
    free(sensor3);
    free(sensor4);

    pthread_exit(NULL);
}

//Insert or Update Table room and sensor_measurements
void* update_sensor(void *arg) {
    char sql_sensor[MAX_NAME_BUFFER],sql_sensor_historic[MAX_NAME_BUFFER],buffer[10];
    int id;

    //Tempo
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    //Verifica os nomes de cada sensor, insere na sensor_measurements, e atualiza se ja existir na sensor(valores mais recentes de cada sensor)
    for(int i=0; i<NUMBER_ROOMS; i++) {
        pthread_mutex_lock(&mutex_room);
        id = room[i].id;
        pthread_mutex_unlock(&mutex_room);

        pthread_mutex_lock(&mutex_room);
        if (!strcmp(room[id].sensor_name[0],"HUM")) {
            sprintf(buffer,"HUM%d",room[id].sensor_location[0]);
            pthread_mutex_lock(&mutex_db);

            sprintf(sql_sensor_historic,"INSERT INTO sensor_measurements (location,sensor,sensor_name,sensor_id,measure,sensor_time) VALUES ('%s','%s','HUM','%d','%f','%d-%d-%d %d:%d:%d');",what_name(id),buffer,room[id].sensor_location[0],room[id].humidity,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
            PQexec(conn,sql_sensor_historic);

            sprintf(sql_sensor,"UPDATE sensor SET measure='%f' WHERE sensor_name='HUM' AND sensor_id='%d'; INSERT INTO sensor (location,sensor,sensor_name,sensor_id,measure) SELECT '%s','%s','HUM','%d','%f' WHERE NOT EXISTS (SELECT 1 FROM sensor WHERE sensor_name = 'HUM' and sensor_id ='%d');",room[id].humidity,room[id].sensor_location[0],what_name(id),buffer,room[id].sensor_location[0],room[id].humidity,room[id].sensor_location[0]);
            PQexec(conn,sql_sensor);

            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(room[id].sensor_name[1],"LUX")) {
            sprintf(buffer,"LUX%d",room[id].sensor_location[1]);
            pthread_mutex_lock(&mutex_db);

            sprintf(sql_sensor_historic,"INSERT INTO sensor_measurements (location,sensor,sensor_name,sensor_id,measure,sensor_time) VALUES ('%s','%s','LUX','%d','%f','%d-%d-%d %d:%d:%d');",what_name(id),buffer,room[id].sensor_location[1],room[id].light,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
            PQexec(conn,sql_sensor_historic);

            sprintf(sql_sensor,"UPDATE sensor SET measure='%f' WHERE sensor_name='LUX' AND sensor_id='%d'; INSERT INTO sensor (location,sensor,sensor_name,sensor_id,measure) SELECT '%s','%s','LUX','%d','%f' WHERE NOT EXISTS (SELECT 1 FROM sensor WHERE sensor_name = 'LUX' and sensor_id ='%d');",room[id].light,room[id].sensor_location[1],what_name(id),buffer,room[id].sensor_location[1],room[id].light,room[id].sensor_location[1]);
            PQexec(conn,sql_sensor);

            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(room[id].sensor_name[2],"MOV")) {
            sprintf(buffer,"MOV%d",room[id].sensor_location[2]);
            pthread_mutex_lock(&mutex_db);

            sprintf(sql_sensor_historic,"INSERT INTO sensor_measurements (location,sensor,sensor_name,sensor_id,measure,sensor_time) VALUES ('%s','%s','MOV','%d','%f','%d-%d-%d %d:%d:%d');",what_name(id),buffer,room[id].sensor_location[2],room[id].movement,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
            PQexec(conn,sql_sensor_historic);

            sprintf(sql_sensor,"UPDATE sensor SET measure='%f' WHERE sensor_name='MOV' AND sensor_id='%d'; INSERT INTO sensor (location,sensor,sensor_name,sensor_id,measure) SELECT '%s','%s','MOV','%d','%f' WHERE NOT EXISTS (SELECT 1 FROM sensor WHERE sensor_name = 'MOV' and sensor_id ='%d');",room[id].movement,room[id].sensor_location[2],what_name(id),buffer,room[id].sensor_location[2],room[id].movement,room[id].sensor_location[2]);
            PQexec(conn,sql_sensor);

            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(room[id].sensor_name[3],"POT")) {
            sprintf(buffer,"POT%d",room[id].sensor_location[3]);
            pthread_mutex_lock(&mutex_db);

            sprintf(sql_sensor_historic,"INSERT INTO sensor_measurements (location,sensor,sensor_name,sensor_id,measure,sensor_time) VALUES ('%s','%s','POT','%d','%f','%d-%d-%d %d:%d:%d');",what_name(id),buffer,room[id].sensor_location[3],room[id].power,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
            PQexec(conn,sql_sensor_historic);

            sprintf(sql_sensor,"UPDATE sensor SET measure='%f' WHERE sensor_name='POT' AND sensor_id='%d'; INSERT INTO sensor (location,sensor,sensor_name,sensor_id,measure) SELECT '%s','%s','POT','%d','%f' WHERE NOT EXISTS (SELECT 1 FROM sensor WHERE sensor_name = 'POT' and sensor_id ='%d');",room[id].power,room[id].sensor_location[3],what_name(id),buffer,room[id].sensor_location[3],room[id].power,room[id].sensor_location[3]);
            PQexec(conn,sql_sensor);

            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(room[id].sensor_name[4],"SMOKE")) {
            sprintf(buffer,"SMOKE%d",room[id].sensor_location[4]);
            pthread_mutex_lock(&mutex_db);

            sprintf(sql_sensor_historic,"INSERT INTO sensor_measurements (location,sensor,sensor_name,sensor_id,measure,sensor_time) VALUES ('%s','%s','SMOKE','%d','%f','%d-%d-%d %d:%d:%d');",what_name(id),buffer,room[id].sensor_location[4],room[id].smoke,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
            PQexec(conn,sql_sensor_historic);

            sprintf(sql_sensor,"UPDATE sensor SET measure='%f' WHERE sensor_name='SMOKE' AND sensor_id='%d'; INSERT INTO sensor (location,sensor,sensor_name,sensor_id,measure) SELECT '%s','%s','SMOKE','%d','%f' WHERE NOT EXISTS (SELECT 1 FROM sensor WHERE sensor_name = 'SMOKE' and sensor_id ='%d');",room[id].smoke,room[id].sensor_location[4],what_name(id),buffer,room[id].sensor_location[4],room[id].smoke,room[id].sensor_location[4]);
            PQexec(conn,sql_sensor);

            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(room[id].sensor_name[5],"SOUND")) {
            sprintf(buffer,"SOUND%d",room[id].sensor_location[5]);
            pthread_mutex_lock(&mutex_db);

            sprintf(sql_sensor_historic,"INSERT INTO sensor_measurements (location,sensor,sensor_name,sensor_id,measure,sensor_time) VALUES ('%s','%s','SOUND','%d','%f','%d-%d-%d %d:%d:%d');",what_name(id),buffer,room[id].sensor_location[5],room[id].sound,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
            PQexec(conn,sql_sensor_historic);

            sprintf(sql_sensor,"UPDATE sensor SET measure='%f' WHERE sensor_name='SOUND' AND sensor_id='%d'; INSERT INTO sensor (location,sensor,sensor_name,sensor_id,measure) SELECT '%s','%s','SOUND','%d','%f' WHERE NOT EXISTS (SELECT 1 FROM sensor WHERE sensor_name = 'SOUND' and sensor_id ='%d');",room[id].sound,room[id].sensor_location[5],what_name(id),buffer,room[id].sensor_location[5],room[id].sound,room[id].sensor_location[5]);
            PQexec(conn,sql_sensor);

            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(room[id].sensor_name[6],"TEMP")) {
            sprintf(buffer,"TEMP%d",room[id].sensor_location[6]);
            pthread_mutex_lock(&mutex_db);

            sprintf(sql_sensor_historic,"INSERT INTO sensor_measurements (location,sensor,sensor_name,sensor_id,measure,sensor_time) VALUES ('%s','%s','TEMP','%d','%f','%d-%d-%d %d:%d:%d');",what_name(id),buffer,room[id].sensor_location[6],room[id].temperature,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
            PQexec(conn,sql_sensor_historic);

            sprintf(sql_sensor,"UPDATE sensor SET measure='%f' WHERE sensor_name='TEMP' AND sensor_id='%d'; INSERT INTO sensor (location,sensor,sensor_name,sensor_id,measure) SELECT '%s','%s','TEMP','%d','%f' WHERE NOT EXISTS (SELECT 1 FROM sensor WHERE sensor_name = 'TEMP' and sensor_id ='%d');",room[id].temperature,room[id].sensor_location[6],what_name(id),buffer,room[id].sensor_location[6],room[id].temperature,room[id].sensor_location[6]);
            PQexec(conn,sql_sensor);

            pthread_mutex_unlock(&mutex_db);
        }
        pthread_mutex_unlock(&mutex_room);
    }
}

//- TRUE VALUES FOR EACH ROOM 
void* true_sensors_values(void* arg) {

    for(int i=0; i<NUMBER_ROOMS; i++) {
        pthread_mutex_lock(&mutex_sensors);
        pthread_mutex_lock(&mutex_room);
        if (sensors[i].id==0) {
            room[0].id = 0;
            room[0].temperature = real_temperature(sensors[home[0].sensores[0][4] - '0'].sensor3);
            room[0].sound = detect_sound(sensors[home[0].sensores[1][5] - '0'].sensor4);
            strcpy(room[0].sensor_name[6],"TEMP");
            strcpy(room[0].sensor_name[5],"SOUND");
            room[0].sensor_location[6]=home[0].sensores[0][4] - '0';
            room[0].sensor_location[5]=home[0].sensores[1][5] - '0';
        }
        else if (sensors[i].id==1) {
            room[1].id = 1;
            room[1].temperature = real_temperature(sensors[home[1].sensores[0][4] - '0'].sensor3);
            strcpy(room[1].sensor_name[6],"TEMP");
            room[1].sensor_location[6]=home[1].sensores[0][4] - '0';
        }
        else if (sensors[i].id==3) {
            room[3].id = 3;
            room[3].temperature = real_temperature(sensors[home[2].sensores[0][4] - '0'].sensor3);
            strcpy(room[3].sensor_name[6],"TEMP");
            room[3].sensor_location[6]=home[2].sensores[0][4] - '0';
        }
        else if (sensors[i].id==5) {
            room[5].id = 5;
            room[5].movement = detect_movement(sensors[home[3].sensores[2][3] - '0'].sensor1);
            room[5].temperature = real_temperature(sensors[home[3].sensores[0][4] - '0'].sensor3);
            room[5].humidity = relative_humidity(sensors[home[3].sensores[1][3] - '0'].sensor4);
            strcpy(room[5].sensor_name[2],"MOV");
            strcpy(room[5].sensor_name[6],"TEMP");
            strcpy(room[5].sensor_name[0],"HUM");
            room[5].sensor_location[2]=home[3].sensores[2][3] - '0';
            room[5].sensor_location[6]=home[3].sensores[0][4] - '0';
            room[5].sensor_location[0]=home[3].sensores[1][3] - '0';
        }
        else if (sensors[i].id==6) {
            room[6].id = 6;
            room[6].light = detect_light(sensors[home[4].sensores[1][3] - '0'].sensor1);
            room[6].temperature = real_temperature(sensors[home[4].sensores[0][4] - '0'].sensor3);
            strcpy(room[6].sensor_name[1],"LUX");
            strcpy(room[6].sensor_name[6],"TEMP");
            room[6].sensor_location[1]=home[4].sensores[1][3] - '0';
            room[6].sensor_location[6]=home[4].sensores[0][4] - '0';
        }
        else if (sensors[i].id==7) {
            room[7].id = 7;
            room[7].temperature = real_temperature(sensors[home[5].sensores[0][4] - '0'].sensor3);
            strcpy(room[7].sensor_name[6],"TEMP");
            room[7].sensor_location[6]=home[5].sensores[0][4] - '0';
        }
        else if (sensors[i].id==8) {
            room[8].id = 8;
            room[8].humidity = relative_humidity(sensors[home[6].sensores[0][3] - '0'].sensor4);
            room[8].voltage = real_voltage(sensors[home[6].sensores[1][3] - '0'].voltage,220);
            room[8].current = real_current(sensors[home[6].sensores[1][3] - '0'].current);
            room[8].power = dissipated_power(220,room[8].current);
            strcpy(room[8].sensor_name[0],"HUM");
            strcpy(room[8].sensor_name[3],"POT");
            room[8].sensor_location[0]=home[6].sensores[0][3] - '0';
            room[8].sensor_location[3]=home[6].sensores[1][3] - '0';
        }
        else if (sensors[i].id==9) {
            room[9].id = 9;
            room[9].movement = detect_movement(sensors[home[7].sensores[1][3] - '0'].sensor1);
            room[9].smoke = percentage_smoke(sensors[home[7].sensores[2][5] - '0'].sensor3);
            room[9].humidity = relative_humidity(sensors[home[7].sensores[0][3] - '0'].sensor4);
            room[9].voltage = real_voltage(sensors[home[7].sensores[3][3] - '0'].voltage,220);
            room[9].current = real_current(sensors[home[7].sensores[3][3] - '0'].current);
            room[9].power = dissipated_power(220,room[9].current);
            strcpy(room[9].sensor_name[2],"MOV");
            strcpy(room[9].sensor_name[4],"SMOKE");
            strcpy(room[9].sensor_name[0],"HUM");
            strcpy(room[9].sensor_name[3],"POT");
            room[9].sensor_location[2]=home[7].sensores[1][3] - '0';
            room[9].sensor_location[4]=home[7].sensores[2][5] - '0';
            room[9].sensor_location[0]=home[7].sensores[0][3] - '0';
            room[9].sensor_location[3]=home[7].sensores[3][3] - '0';
        }
        else if (sensors[i].id==2) {
            room[2].id = 2;
            room[2].movement = detect_movement(sensors[home[8].sensores[1][3] - '0'].sensor1);
            room[2].humidity = relative_humidity(sensors[home[8].sensores[0][3] - '0'].sensor4);
            strcpy(room[2].sensor_name[2],"MOV");
            strcpy(room[2].sensor_name[0],"HUM");
            strcpy(room[2].sensor_name[4],"SEC");
            room[2].sensor_location[2]=home[8].sensores[1][3] - '0';
            room[2].sensor_location[0]=home[8].sensores[0][3] - '0';
            room[2].sensor_location[4]=10;
        }
        else if (sensors[i].id==4) {
            room[4].id = 4;
            room[4].humidity = relative_humidity(sensors[home[9].sensores[0][3] - '0'].sensor4);
            strcpy(room[4].sensor_name[0],"HUM");
            strcpy(room[4].sensor_name[4],"SEC");
            room[4].sensor_location[0]=home[9].sensores[0][3] - '0';
            room[4].sensor_location[4]=10;
        }
        pthread_mutex_unlock(&mutex_room);
        pthread_mutex_unlock(&mutex_sensors);
	}

    //Questoes de segurança de codigo
    pthread_mutex_lock(&mutex_room);
    room[2].id = 2;
    room[4].id = 4;
    pthread_mutex_unlock(&mutex_room);

    pthread_exit(NULL);
}

//- VALOR ABSOLUTO
float valorAbsoluto (float x) {
    if(x>=0.0) return x;
    else return -x;
}

//- CONTROLADOR P
int P_controller (float temperature) {
    //Declive maximo de 2, devido ao intervalo de valores colocado no MsgCreatorConf
    float *declive;
    declive = (float*)malloc(sizeof(float));
    *declive = KP*(temperature-T_IDEAL);
    *declive = valorAbsoluto(*declive);
    
    if (*declive>0 && *declive <= 1) {free(declive); return 1;}
    else if (*declive>1 && *declive<= 2) {free(declive); return 2;}
    else {free(declive); return 1;}
}   

//- MUDAR DECLIVE do gráfico linear da temperatura, quando o atuador HEATER ou COOL atuam
void change_temperature_slope1 (int declive1) {
    //"r+" é a unica maneira segura de escrever para o ficheiro .txt
    f_msgcreator1= fopen("MsgCreatorConf1.txt","r+");

    //"\n" usada por questões de segurança a escrever para o ficheiro
    if (declive1==1) { fprintf(f_msgcreator1,"-n 5 -l 100 -f 5 -c 1 -s [1,3,4] -d [['C',0.0,100.0,1],['L',16.0,24.0,1],['C',0.0,100.0,3]] -i 0\n");  }
    else if (declive1==-1) { fprintf(f_msgcreator1,"-n 5 -l 100 -f 5 -c 1 -s [1,3,4] -d [['C',0.0,100.0,1],['L',16.0,24.0,-1],['C',0.0,100.0,3]] -i 0");  }
    else if (declive1==2) { fprintf(f_msgcreator1,"-n 5 -l 100 -f 5 -c 1 -s [1,3,4] -d [['C',0.0,100.0,1],['L',16.0,24.0,2],['C',0.0,100.0,3]] -i 0\n");  }
    else if (declive1==-2) { fprintf(f_msgcreator1,"-n 5 -l 100 -f 5 -c 1 -s [1,3,4] -d [['C',0.0,100.0,1],['L',16.0,24.0,-2],['C',0.0,100.0,3]] -i 0");  }
    
    fclose(f_msgcreator1);
}

void change_temperature_slope2 (int declive2) {
    f_msgcreator2= fopen("MsgCreatorConf2.txt","r+");

    if (declive2==1) { fprintf(f_msgcreator2,"-n 2 -l 100 -f 5 -c 1 -s [1,3,4] -d [['U',0.0,100.0,0.01],['L',16.0,24.0,1],['C',0.0,100.0,3]] -i 5\n");  }
    else if (declive2==-1) { fprintf(f_msgcreator2,"-n 2 -l 100 -f 5 -c 1 -s [1,3,4] -d [['U',0.0,100.0,0.01],['L',16.0,24.0,-1],['C',0.0,100.0,3]] -i 5");  }
    else if (declive2==2) { fprintf(f_msgcreator2,"-n 2 -l 100 -f 5 -c 1 -s [1,3,4] -d [['U',0.0,100.0,0.01],['L',16.0,24.0,2],['C',0.0,100.0,3]] -i 5\n");  }
    else if (declive2==-2) { fprintf(f_msgcreator2,"-n 2 -l 100 -f 5 -c 1 -s [1,3,4] -d [['U',0.0,100.0,0.01],['L',16.0,24.0,-2],['C',0.0,100.0,3]] -i 5");  }
    
    fclose(f_msgcreator2);
}

//- CHECK RULES
void* checkRules(void *num_rules) {
    int *number_rules = (int*) num_rules;
    int aux,aux_declive1,aux_declive2,hora_atual,min_atual;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    hora_atual = tm.tm_hour;
    min_atual  =tm.tm_min;
    
    for(int i=0; i<= (*number_rules) ; i++) {
        //Regra da Temperatura
        if (!strcmp(file_rules[i].sensor,"TEMP")) {
            if((room[file_rules[i].id].temperature <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1) ) ) {
                if (!strcmp(file_rules[i].actuator,"HEATER")) {
                    pthread_mutex_lock(&mutex_actuators);
                    actuators[file_rules[i].id].id = file_rules[i].id;
                    strcpy(actuators[file_rules[i].id].name[2],"HEATER");
                    actuators[file_rules[i].id].heater = true; 
                    pthread_mutex_unlock(&mutex_actuators);
                }
                else if (!strcmp(file_rules[i].actuator,"COOL")) {
                    pthread_mutex_lock(&mutex_actuators);
                    actuators[file_rules[i].id].id = file_rules[i].id;
                    strcpy(actuators[file_rules[i].id].name[0],"COOL");
                    actuators[file_rules[i].id].cool = false;
                    pthread_mutex_unlock(&mutex_actuators);
                }
            }
            else if ((room[file_rules[i].id].temperature > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1) ) ) {
                if (!strcmp(file_rules[i].actuator,"HEATER")) {
                    pthread_mutex_lock(&mutex_actuators);
                    actuators[file_rules[i].id].id = file_rules[i].id;
                    strcpy(actuators[file_rules[i].id].name[2],"HEATER");
                    actuators[file_rules[i].id].heater = false;
                    pthread_mutex_unlock(&mutex_actuators);
                }
                else if (!strcmp(file_rules[i].actuator,"COOL")) {
                    pthread_mutex_lock(&mutex_actuators);
                    actuators[file_rules[i].id].id = file_rules[i].id;
                    strcpy(actuators[file_rules[i].id].name[0],"COOL");
                    actuators[file_rules[i].id].cool = true; 
                    pthread_mutex_unlock(&mutex_actuators);
                }
            }
            //Maquina de Estados
            pthread_mutex_lock(&mutex_actuators);
            //Variável auxiliar usada para guardar o valor do id do atuador
            aux = file_rules[i].id;
            if ( (actuators[aux].id == 0) || (actuators[aux].id == 1) || (actuators[aux].id == 3) || (actuators[aux].id == 6) || (actuators[aux].id == 7) ) {
                //declive após verificação do controlador P 
                aux_declive1 = P_controller(room[aux].temperature);
                if (state_temp1==0 && actuators[aux].cool && !actuators[aux].heater) { state_temp1=1; slope_cool1=1; change_temperature_slope1(-aux_declive1); }
                else if (state_temp1==1 && slope_cool1==1) { state_temp1=2; slope_cool1=0; }
                else if (state_temp1==2 && !actuators[aux].cool && actuators[aux].heater ) { state_temp1=3; slope_heater1=1; change_temperature_slope1(aux_declive1); }
                else if (state_temp1==3 && slope_heater1==1 ) { state_temp1=0; slope_heater1=0; }  
            }
            else if ( (actuators[aux].id == 5) || (actuators[aux].id == 8) ) {
                aux_declive2 = P_controller(room[aux].temperature);
                if (state_temp2==0 && actuators[aux].cool && !actuators[aux].heater) { state_temp2=1; slope_cool2=1; change_temperature_slope2(-aux_declive2); }
                else if (state_temp2==1 && slope_cool2==1) { state_temp2=2; slope_cool2=0; }
                else if (state_temp2==2 && !actuators[aux].cool && actuators[aux].heater ) { state_temp2=3; slope_heater2=1; change_temperature_slope2(aux_declive2); }
                else if (state_temp2==3 && slope_heater2==1 ) { state_temp2=0; slope_heater2=0; } 
            }
            pthread_mutex_unlock(&mutex_actuators);
        }
        //Regra do sensor de som
        else if (!strcmp(file_rules[i].sensor,"SOUND") ) {
            if ( (room[file_rules[i].id].sound > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1) ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[7],"WARNSO");
                actuators[file_rules[i].id].warnso = true; 
                pthread_mutex_unlock(&mutex_actuators);   
            }
            else if ( (room[file_rules[i].id].sound <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1) ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[7],"WARNSO");
                actuators[file_rules[i].id].warnso = false;
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra da Humidade
        else if (!strcmp(file_rules[i].sensor,"HUM")) {
            if ( (room[file_rules[i].id].humidity <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1) ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[1],"DEHUM");
                actuators[file_rules[i].id].dehum = false;
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].humidity > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[1],"DEHUM");
                actuators[file_rules[i].id].dehum = true; 
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra do Movimento
        else if (!strcmp(file_rules[i].sensor,"MOV")) {
            if ( (room[file_rules[i].id].movement <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[5],"WARNM");
                actuators[file_rules[i].id].warnm = false; 
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].movement > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[5],"WARNM");
                actuators[file_rules[i].id].warnm = true; 
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra da Luz
        else if (!strcmp(file_rules[i].sensor,"LUX")) {
            if ( (room[file_rules[i].id].light <= file_rules[i].valors)&& (!strcmp(file_rules[i].operator,"<=")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[3],"LAMP");
                actuators[file_rules[i].id].lamp = true;
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].light > file_rules[i].valors)&& (!strcmp(file_rules[i].operator,">")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {    
                pthread_mutex_lock(&mutex_actuators);                   
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[3],"LAMP");
                actuators[file_rules[i].id].lamp = false;
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra da potencia
        else if (!strcmp(file_rules[i].sensor,"POT")) {
            if ( (room[file_rules[i].id].power <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[4],"POTD");
                actuators[file_rules[i].id].potd = false;  
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].power > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[4],"POTD");
                actuators[file_rules[i].id].potd = true;  
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra do fumo
        else if (!strcmp(file_rules[i].sensor,"SMOKE")) {
            if ( (room[file_rules[i].id].smoke <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[6],"WARNSM");
                actuators[file_rules[i].id].warsm = false; 
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].smoke > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) && ( (file_rules[i].start_hour>=0 && (file_rules[i].start_hour < hora_atual || (file_rules[i].start_hour==hora_atual && file_rules[i].start_min>=min_atual)) && (file_rules[i].end_hour > hora_atual || (file_rules[i].end_hour==hora_atual && file_rules[i].end_min > min_atual)) && file_rules[i].end_hour >= 0) || (file_rules[i].start_hour==-1 && file_rules[i].end_hour==-1)  ) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name[6],"WARNSM");
                actuators[file_rules[i].id].warsm = true; 
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
    }
    pthread_exit(NULL);
} 

//- Insert or Update Tables actuator and actuator_state
void* update_actuator(void* arg){
    char sql_actuator[MAX_NAME_BUFFER],sql_actuator_state[MAX_NAME_BUFFER],buffer_actuator[15];
    int id;

    //Tempo real
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    for(int i=0; i<NUMBER_ROOMS; i++) {
        pthread_mutex_lock(&mutex_actuators);
        id = actuators[i].id;

        //Verifica os nomes de cada atuador, insere na actuator_state, e atualiza se ja existir na actuator(valores mais recentes de cada actuador)
        if (!strcmp(actuators[id].name[0],"COOL")) {
            sprintf(buffer_actuator,"COOL%d",id);
            if( state_actuator[i][0]==0 && !actuators[i].cool) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','COOL','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].cool,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][0]=1; 
            }
            else if (state_actuator[i][0]==1 && actuators[i].cool) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','COOL','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].cool,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][0]=0; 
            }
            pthread_mutex_lock(&mutex_db);
            sprintf(sql_actuator,"UPDATE actuator SET actuator_state='%d' WHERE actuator_name='COOL' AND actuator_id='%d'; INSERT INTO actuator (actuator,actuator_name,actuator_id,actuator_state) SELECT '%s','COOL','%d','%d' WHERE NOT EXISTS (SELECT 1 FROM actuator WHERE actuator_name = 'COOL' and actuator_id ='%d');",actuators[id].cool,id,buffer_actuator,id,actuators[id].cool,id);
            PQexec(conn,sql_actuator);
            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(actuators[id].name[1],"DEHUM")) {
            sprintf(buffer_actuator,"DEHUM%d",id);
            if( state_actuator[i][1]==0 && !actuators[i].dehum) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','DEHUM','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].dehum,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
                PQexec(conn,sql_actuator_state); 
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][1]=1; 
            }
            else if (state_actuator[i][1]==1 && actuators[i].dehum) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','DEHUM','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].dehum,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][1]=0; 
            }
            pthread_mutex_lock(&mutex_db);
            sprintf(sql_actuator,"UPDATE actuator SET actuator_state='%d' WHERE actuator_name='DEHUM' AND actuator_id='%d'; INSERT INTO actuator (actuator,actuator_name,actuator_id,actuator_state) SELECT '%s','DEHUM','%d','%d' WHERE NOT EXISTS (SELECT 1 FROM actuator WHERE actuator_name = 'DEHUM' and actuator_id ='%d');",actuators[id].dehum,id,buffer_actuator,id,actuators[id].dehum,id);
            PQexec(conn,sql_actuator);

            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(actuators[id].name[2],"HEATER")) {
            sprintf(buffer_actuator,"HEATER%d",id);
            if( state_actuator[i][2]==0 && !actuators[i].heater) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','HEATER','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].heater,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][2]=1; 
            }
            else if (state_actuator[i][2]==1 && actuators[i].heater) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','HEATER','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].heater,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][2]=0; 
            }

            pthread_mutex_lock(&mutex_db);
            sprintf(sql_actuator,"UPDATE actuator SET actuator_state='%d' WHERE actuator_name='HEATER' AND actuator_id='%d'; INSERT INTO actuator (actuator,actuator_name,actuator_id,actuator_state) SELECT '%s','HEATER','%d','%d' WHERE NOT EXISTS (SELECT 1 FROM actuator WHERE actuator_name = 'HEATER' and actuator_id ='%d');",actuators[id].heater,id,buffer_actuator,id,actuators[id].heater,id);
            PQexec(conn,sql_actuator);

            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(actuators[id].name[3],"LAMP")) {
            sprintf(buffer_actuator,"LAMP%d",id);
            if( state_actuator[i][3]==0 && !actuators[i].lamp) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','LAMP','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].lamp,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][3]=1; 
            }
            else if (state_actuator[i][3]==1 && actuators[i].lamp) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','LAMP','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].lamp,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][3]=0; 
            }

            pthread_mutex_lock(&mutex_db);
            sprintf(sql_actuator,"UPDATE actuator SET actuator_state='%d' WHERE actuator_name='LAMP' AND actuator_id='%d'; INSERT INTO actuator (actuator,actuator_name,actuator_id,actuator_state) SELECT '%s','LAMP','%d','%d' WHERE NOT EXISTS (SELECT 1 FROM actuator WHERE actuator_name = 'LAMP' and actuator_id ='%d');",actuators[id].lamp,id,buffer_actuator,id,actuators[id].lamp,id);
            PQexec(conn,sql_actuator);
            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(actuators[id].name[4],"POTD")) {
            sprintf(buffer_actuator,"POTD%d",id);
            if( state_actuator[i][4]==0 && !actuators[i].potd) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','POTD','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].potd,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][4]=1; 
            }
            else if (state_actuator[i][4]==1 && actuators[i].potd) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','POTD','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].potd,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][4]=0; 
            }

            pthread_mutex_lock(&mutex_db);
            sprintf(sql_actuator,"UPDATE actuator SET actuator_state='%d' WHERE actuator_name='POTD' AND actuator_id='%d'; INSERT INTO actuator (actuator,actuator_name,actuator_id,actuator_state) SELECT '%s','POTD','%d','%d' WHERE NOT EXISTS (SELECT 1 FROM actuator WHERE actuator_name = 'POTD' and actuator_id ='%d');",actuators[id].potd,id,buffer_actuator,id,actuators[id].potd,id);
            PQexec(conn,sql_actuator);
            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(actuators[id].name[5],"WARNM")) {
            sprintf(buffer_actuator,"WARNM%d",id);
            if( state_actuator[i][5]==0 && !actuators[i].warnm) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','WARNM','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].warnm,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][5]=1; 
            }
            else if (state_actuator[i][5]==1 && actuators[i].warnm) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','WARNM','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].warnm,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][5]=0; 
            }

            pthread_mutex_lock(&mutex_db);
            sprintf(sql_actuator,"UPDATE actuator SET actuator_state='%d' WHERE actuator_name='WARNM' AND actuator_id='%d'; INSERT INTO actuator (actuator,actuator_name,actuator_id,actuator_state) SELECT '%s','WARNM','%d','%d' WHERE NOT EXISTS (SELECT 1 FROM actuator WHERE actuator_name = 'WARNM' and actuator_id ='%d');",actuators[id].warnm,id,buffer_actuator,id,actuators[id].warnm,id);
            PQexec(conn,sql_actuator);
            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(actuators[id].name[6],"WARNSM")) {
            sprintf(buffer_actuator,"WARNSM%d",id);
            if( state_actuator[i][6]==0 && !actuators[i].warsm) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','WARNSM','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].warsm,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][6]=1; 
            }
            else if (state_actuator[i][6]==1 && actuators[i].warsm) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','WARNSM','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].warsm,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][6]=0; 
            }

            pthread_mutex_lock(&mutex_db);
            sprintf(sql_actuator,"UPDATE actuator SET actuator_state='%d' WHERE actuator_name='WARNSM' AND actuator_id='%d'; INSERT INTO actuator (actuator,actuator_name,actuator_id,actuator_state) SELECT '%s','WARNSM','%d','%d' WHERE NOT EXISTS (SELECT 1 FROM actuator WHERE actuator_name = 'WARNSM' and actuator_id ='%d');",actuators[id].warsm,id,buffer_actuator,id,actuators[id].warsm,id);
            PQexec(conn,sql_actuator);
            pthread_mutex_unlock(&mutex_db);
        }
        if (!strcmp(actuators[id].name[7],"WARNSO")) {
            sprintf(buffer_actuator,"WARNSO%d",id);
            if( state_actuator[i][7]==0 && !actuators[i].warnso) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','WARNSO','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].warnso,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][7]=1; 
            }
            else if (state_actuator[i][7]==1 && actuators[i].warnso) { 
                pthread_mutex_lock(&mutex_db);
                sprintf(sql_actuator_state,"INSERT INTO actuator_state (actuator,actuator_name,actuator_id,actuator_state,actuator_time) VALUES ('%s','WARNSO','%d','%d','%d-%d-%d %d:%d:%d');",buffer_actuator,id,actuators[id].warnso,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec); 
                PQexec(conn,sql_actuator_state);
                pthread_mutex_unlock(&mutex_db);
                state_actuator[i][7]=0; 
            }

            pthread_mutex_lock(&mutex_db);
            sprintf(sql_actuator,"UPDATE actuator SET actuator_state='%d' WHERE actuator_name='WARNSO' AND actuator_id='%d'; INSERT INTO actuator (actuator,actuator_name,actuator_id,actuator_state) SELECT '%s','WARNSO','%d','%d' WHERE NOT EXISTS (SELECT 1 FROM actuator WHERE actuator_name = 'WARNSO' and actuator_id ='%d');",actuators[id].warnso,id,buffer_actuator,id,actuators[id].warnso,id);
            PQexec(conn,sql_actuator);
            pthread_mutex_unlock(&mutex_db);
        }
       
        pthread_mutex_unlock(&mutex_actuators);
    }
}

//- INIT RGB, ALL WHITE
void init_rgb(void) {
    C1=BRANCO;
    C2=BRANCO;
    C3=BRANCO;
    C4=BRANCO;
    C5=BRANCO;
    C6=BRANCO;
    C7=BRANCO;
    C8=BRANCO;
    C9=BRANCO;
    C10=BRANCO;
    C11=BRANCO;
    C12=BRANCO;
    C13=BRANCO;
    C14=BRANCO;
    C15=BRANCO;
    C16=BRANCO;
    C17=BRANCO;
    C18=BRANCO;
    C19=BRANCO;
    C20=BRANCO;
    C21=BRANCO;
    C22=BRANCO;
    C23=BRANCO;
    C24=BRANCO;
    C25=BRANCO;
}

//- Write on RGB
void* writeRGBMatrix(void* arg) {
    pthread_mutex_lock(&mutex_actuators);
    if(actuators[0].heater) {C1=VERMELHO;}
    else {C1=BRANCO;}
    if( actuators[0].cool) {C2=AZUL;}
    else {C2=BRANCO;}
    if(actuators[0].warnso) {C3=PRETO;}
    else {C3=BRANCO;}

    if(actuators[1].heater) {C4=VERMELHO;}
    else {C4=BRANCO;}
    if( actuators[1].cool) {C5=AZUL;}
    else {C5=BRANCO;}

    if(actuators[3].heater) {C6=VERMELHO;}
    else {C6=BRANCO;}
    if( actuators[3].cool) {C7=AZUL;}
    else {C7=BRANCO;}
        
    if( actuators[5].heater) {C8=VERMELHO;}
    else  {C8=BRANCO;}
    if( actuators[5].cool) {C9=AZUL;}
    else  {C9=BRANCO;}
    if( actuators[5].dehum) {C10=CASTANHO;}
    else {C10=BRANCO;}
    if( actuators[5].warnm) {C11=VERDE;}
    else  {C11=BRANCO;}

    if( actuators[6].heater) {C12=VERMELHO;}
    else {C12=BRANCO;}
    if( actuators[6].cool) {C13=AZUL;}
    else {C13=BRANCO;}
    if( actuators[6].lamp) {C14=AMARELO;}
    else {C14=BRANCO;}

    if(actuators[7].heater) {C15=VERMELHO;}
    else {C15=BRANCO;}
    if( actuators[7].cool) {C16=AZUL;}
    else {C16=BRANCO;}

    if( actuators[8].dehum) {C17=CASTANHO;}
    else {C17=BRANCO;}
    if( actuators[8].potd) {C18=LARANJA;}
    else {C18=BRANCO;}

    if (actuators[9].dehum) {C19=CASTANHO;}
    else {C19=BRANCO;}
    if (actuators[9].potd) {C20=LARANJA;}
    else {C20=BRANCO;}
    if( actuators[9].warsm) {C21=CINZENTO;}
    else  {C21=BRANCO;}
    if( actuators[9].warnm) {C22=VERDE;}
    else  {C22=BRANCO;}

    if (actuators[4].dehum && adicional==2) { C23=CASTANHO;}
    else {C23=BRANCO;}

    if (actuators[2].dehum && adicional==1) {C24=CASTANHO;}
    else {C24=BRANCO;}
    if( actuators[2].warnm && adicional==1) {C25=VERDE;}
    else  {C25=BRANCO;}
    pthread_mutex_unlock(&mutex_actuators);

    printf("[%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s]\n",C1,C3,C8,C10,C19,C2,C23,C9,C11,C20,C24,C12,C13,C14,C25,C4,C6,C15,C17,C21,C5,C7,C16,C18,C22);

    pthread_exit(NULL);
}
