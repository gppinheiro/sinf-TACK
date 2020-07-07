//GABRIEL OUTEIRO, GUILHERME PINHEIRO & TOMAS ARAUJO -> TACK -> SINF -> 2020 2S
//LIBRARIES
#include "sinf_interfaces.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

//DEFINES
#define MAX_NAME_SZ 256
#define NUMBER_ROOMS 10
#define NUMBER_CELLS 5
#define KP 0.01
#define T_IDEAL 20

//GLOBAL VARIABLES
char freq[3], nmrLeituras[3];
char *C1, *C2, *C3, *C4, *C5, *C6, *C7, *C8, *C9, *C10, *C11, *C11, *C12, *C13, *C14, *C15, *C16, *C17, *C18, *C19, *C20, *C21, *C22, *C23, *C24, *C25;
int slope_heater1, slope_heater2, slope_cool1, slope_cool2, numero_sensores_total, numero_atuadores_total, adicional;

struct House home[NUMBER_ROOMS];
struct Measurements sensors[NUMBER_ROOMS];
struct Motes room[NUMBER_ROOMS];
struct Actuators actuators[50];
struct Rules *file_rules;

FILE *f_msgcreator1,*f_msgcreator2;

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
//- HOW MANY MOTES?
int nmr_motes(int terminal) {
    char stringConf[MAX_NAME_SZ];
    char* motes=(char*)malloc(3 * sizeof(char));

    FILE *fp;
    if(terminal==1) {fp=fopen("MsgCreatorConf.txt","r");}
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
    free(motes);

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

//- READ CONFIG FILE
void* readConfigurations(void* num_divisorias) {
    //Variaveis a usar nesta função
    int i=0,j=0,w=0;
    char linha[MAX_NAME_SZ], *first_token, *second_token, *third_token, *rest;

    //Passar argumento void para int, como queremos
    int *n_divisorias = (int*) num_divisorias;

    //Alocar memória
    first_token = (char*)malloc(30*sizeof(char));
    second_token = (char*)malloc(30*sizeof(char));
    third_token = (char*)malloc(30*sizeof(char));

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
        (*n_divisorias)++;
    }

    //Fechar a stream do ficheiro
    fclose(f_configuration);

    //Variável adicional para saber quantos quartos a mais foram adicionados
    if((*n_divisorias) == 9){adicional=1;}
    else if((*n_divisorias)==10){adicional=2;}
    else adicional=0;

    //Libertar memoria
    free(first_token);
    free(second_token);
    free(third_token);

    pthread_exit(NULL);
}

//- READ RULES FILE    
void* readRules(void* num_rules) {
    //Variaveis a usar nesta função
    char *first_token,*second_token,*third_token,linha[MAX_NAME_SZ];
    int i=0,j=0,w=0;

    //Passar argumento void para int, como queremos
    int *number_rules = (int*) num_rules;

    //Alocar memoria
    first_token = (char*)malloc(30*sizeof(char));
    second_token = (char*)malloc(30*sizeof(char));
    third_token = (char*)malloc(30*sizeof(char));
    file_rules=(struct Rules *)malloc(55*sizeof(struct Rules));
    
    //Abrir ficheiro das regras
    FILE *f_rules= fopen("Sensor_Rules","r");
    
    //Ler ficheiro completo
    while(fgets(linha,MAX_NAME_SZ,f_rules) != NULL) {
        //Aumenta struct
        file_rules=realloc(file_rules,(*number_rules)*sizeof(struct Rules)+1);

        //Le primeiro elemento
        first_token = strtok(linha,":");
        
        //Incrementador a 0, para dividirmos a linha em 3
        i=0;
        while(first_token!=NULL) {
            if(i==0){ strcpy(file_rules[*number_rules].name,first_token); }
            else if(i==1){strcpy(file_rules[*number_rules].cond_sensor,first_token);} 
            else if(i==2){strcpy(file_rules[*number_rules].cond_actuator,first_token);}               
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
                if(!strcmp(third_token,"ON;\n")) { file_rules[*number_rules].on = true; }
                else if(!strcmp(third_token,"OFF;\n")) { file_rules[*number_rules].on = false; }
            }
            w++;
            third_token = strtok(NULL,"=");
        }
        
        //Numero de regras aumenta
        (*number_rules)++;
    }

    fclose(f_rules);

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
    int *motes_nmr,*id_da_mote,*id;
    //Alocar memoria
    mote_id = (char*)malloc(6*sizeof(char)+1);
    voltage = (char*)malloc(6*sizeof(char)+1);
    sensor1 = (char*)malloc(6*sizeof(char)+1);
    current = (char*)malloc(6*sizeof(char)+1);
    sensor3 = (char*)malloc(6*sizeof(char)+1);
    sensor4 = (char*)malloc(6*sizeof(char)+1);
    motes_nmr = (int*)malloc(sizeof(int));
    id_da_mote = (int*)malloc(sizeof(int));
    id = (int*)malloc(sizeof(int));
    //Inicializar
    motes_nmr=0;id_da_mote=0;id=0;
    
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
            if(counter_pos==5) { /*strcpy(mote_id,token);*/ memcpy(mote_id, token, strlen(token)); }
            else if(counter_pos==6) { /*strcat(mote_id,token);*/ memcpy(mote_id+strlen(token), token, strlen(token)+1); }
            //VOLTAGE
            /*else if(counter_pos==10) { strcpy(voltage,token); }
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
            else if(counter_pos==19) { strcat(sensor4,token); }*/

            counter_pos++;
            token = strtok(NULL," "); 
        }

        printf("%s\n",mote_id);

        id_da_mote=strtol(mote_id,NULL,10);

        printf("%d\n",*id_da_mote);
        
        if (id_da_mote==0) { id = 0; }
        else if (id_da_mote==1) { id = 1; }
        else if (id_da_mote==2) { id = 3; }
        else if (id_da_mote==3) { id = 6; }
        else if (id_da_mote==4) { id = 7; }
        else if (id_da_mote==5) { id = 5; }
        else if (id_da_mote==6) { id = 8; }
        else if (id_da_mote==7) { id = 9; }
        else if (id_da_mote==8) { id = 4; }
        else if (id_da_mote==9) { id = 2; }
        
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
    free(id_da_mote);
    free(motes_nmr);
    free(id);

    pthread_exit(NULL);
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
        }
        else if (sensors[i].id==1) {
            room[1].id = 1;
            room[1].temperature = real_temperature(sensors[home[1].sensores[0][4] - '0'].sensor3);
        }
        else if (sensors[i].id==3) {
            room[3].id = 3;
            room[3].temperature = real_temperature(sensors[home[2].sensores[0][4] - '0'].sensor3);
        }
        else if (sensors[i].id==5) {
            room[5].id = 5;
            room[5].movement = detect_movement(sensors[home[3].sensores[2][3] - '0'].sensor1);
            room[5].temperature = real_temperature(sensors[home[3].sensores[0][4] - '0'].sensor3);
            room[5].humidity = relative_humidity(sensors[home[3].sensores[1][3] - '0'].sensor4);
        }
        else if (sensors[i].id==6) {
            room[6].id = 6;
            room[6].light = detect_light(sensors[home[4].sensores[1][3] - '0'].sensor1);
            room[6].temperature = real_temperature(sensors[home[4].sensores[0][4] - '0'].sensor3);
        }
        else if (sensors[i].id==7) {
            room[7].id = 7;
            room[7].temperature = real_temperature(sensors[home[5].sensores[0][4] - '0'].sensor3);
        }
        else if (sensors[i].id==8) {
            room[8].id = 8;
            room[8].humidity = relative_humidity(sensors[home[6].sensores[0][3] - '0'].sensor4);
            room[8].voltage = real_voltage(sensors[home[6].sensores[1][3] - '0'].voltage,220);
            room[8].current = real_current(sensors[home[6].sensores[1][3] - '0'].current);
            room[8].power = dissipated_power(220,room[8].current);
        }
        else if (sensors[i].id==9) {
            room[9].id = 9;
            room[9].movement = detect_movement(sensors[home[7].sensores[1][3] - '0'].sensor1);
            room[9].smoke = percentage_smoke(sensors[home[7].sensores[2][5] - '0'].sensor3);
            room[9].humidity = relative_humidity(sensors[home[7].sensores[0][3] - '0'].sensor4);
            room[9].voltage = real_voltage(sensors[home[7].sensores[3][3] - '0'].voltage,220);
            room[9].current = real_current(sensors[home[7].sensores[3][3] - '0'].current);
            room[9].power = dissipated_power(220,room[9].current);
        }
        else if (sensors[i].id==4) {
            room[4].id = 4;
            room[4].humidity = relative_humidity(sensors[home[8].sensores[0][3] - '0'].sensor4);
        }
        else if (sensors[i].id==2) {
            room[2].id = 2;
            room[2].movement = detect_movement(sensors[home[9].sensores[1][3] - '0'].sensor1);
            room[2].humidity = relative_humidity(sensors[home[9].sensores[0][3] - '0'].sensor4);
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
    float declive = KP*(temperature-T_IDEAL);
    declive = valorAbsoluto(declive);
    
    if (declive>0 && declive <= 1) return 1;
    else if (declive>1 && declive<= 2) return 2;
    else return 1;
}   

//- MUDAR DECLIVE do gráfico linear da temperatura, quando o atuador HEATER ou COOL atuam
void change_temperature_slope1 (int declive1) {
    //"r+" é a unica maneira segura de escrever para o ficheiro .txt
    f_msgcreator1= fopen("MsgCreatorConf.txt","r+");

    //"\n" usada por questões de segurança a escrever para o ficheiro
    if (declive1==1) { fprintf(f_msgcreator1,"-n 5 -l 100 -f 20 -c 1 -s [1,3,4] -d [['C',0.0,100.0,1],['L',16.0,24.0,1],['C',0.0,100.0,3]] -i 0\n");  }
    else if (declive1==-1) { fprintf(f_msgcreator1,"-n 5 -l 100 -f 20 -c 1 -s [1,3,4] -d [['C',0.0,100.0,1],['L',16.0,24.0,-1],['C',0.0,100.0,3]] -i 0");  }
    else if (declive1==2) { fprintf(f_msgcreator1,"-n 5 -l 100 -f 20 -c 1 -s [1,3,4] -d [['C',0.0,100.0,1],['L',16.0,24.0,2],['C',0.0,100.0,3]] -i 0\n");  }
    else if (declive1==-2) { fprintf(f_msgcreator1,"-n 5 -l 100 -f 20 -c 1 -s [1,3,4] -d [['C',0.0,100.0,1],['L',16.0,24.0,-2],['C',0.0,100.0,3]] -i 0");  }
    
    fclose(f_msgcreator1);
}

void change_temperature_slope2 (int declive2) {
    f_msgcreator2= fopen("MsgCreatorConf2.txt","r+");

    if (declive2==1) { fprintf(f_msgcreator2,"-n 2 -l 100 -f 20 -c 1 -s [1,3,4] -d [['U',0.0,100.0,0.01],['L',16.0,24.0,1],['C',0.0,100.0,3]] -i 5\n");  }
    else if (declive2==-1) { fprintf(f_msgcreator2,"-n 2 -l 100 -f 20 -c 1 -s [1,3,4] -d [['U',0.0,100.0,0.01],['L',16.0,24.0,-1],['C',0.0,100.0,3]] -i 5");  }
    else if (declive2==2) { fprintf(f_msgcreator2,"-n 2 -l 100 -f 20 -c 1 -s [1,3,4] -d [['U',0.0,100.0,0.01],['L',16.0,24.0,2],['C',0.0,100.0,3]] -i 5\n");  }
    else if (declive2==-2) { fprintf(f_msgcreator2,"-n 2 -l 100 -f 20 -c 1 -s [1,3,4] -d [['U',0.0,100.0,0.01],['L',16.0,24.0,-2],['C',0.0,100.0,3]] -i 5");  }
    
    fclose(f_msgcreator2);
}

//- CHECK RULES
void* checkRules(void *num_rules) {
    int *number_rules = (int*) num_rules;
    int aux,aux_declive1,aux_declive2;
    for(int i=0; i<= (*number_rules) ; i++) {
        //Regra da Temperatura
        if (!strcmp(file_rules[i].sensor,"TEMP")) {
            if((room[file_rules[i].id].temperature <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<="))) {
                if (!strcmp(file_rules[i].actuator,"HEATER")) {
                    pthread_mutex_lock(&mutex_actuators);
                    actuators[file_rules[i].id].id = file_rules[i].id;
                    strcpy(actuators[file_rules[i].id].name,"HEATER");
                    actuators[file_rules[i].id].heater = true;
                    pthread_mutex_unlock(&mutex_actuators);
                }
                else if (!strcmp(file_rules[i].actuator,"COOL")) {
                    pthread_mutex_lock(&mutex_actuators);
                    actuators[file_rules[i].id].id = file_rules[i].id;
                    strcpy(actuators[file_rules[i].id].name,"COOL");
                    actuators[file_rules[i].id].cool = false;
                    pthread_mutex_unlock(&mutex_actuators);
                }
            }
            else if ((room[file_rules[i].id].temperature > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">"))) {
                if (!strcmp(file_rules[i].actuator,"HEATER")) {
                    pthread_mutex_lock(&mutex_actuators);
                    actuators[file_rules[i].id].id = file_rules[i].id;
                    strcpy(actuators[file_rules[i].id].name,"HEATER");
                    actuators[file_rules[i].id].heater = false;
                    pthread_mutex_unlock(&mutex_actuators);
                }
                else if (!strcmp(file_rules[i].actuator,"COOL")) {
                    pthread_mutex_lock(&mutex_actuators);
                    actuators[file_rules[i].id].id = file_rules[i].id;
                    strcpy(actuators[file_rules[i].id].name,"COOL");
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
            if ( (room[file_rules[i].id].sound > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">"))) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"WARNSO");
                actuators[file_rules[i].id].warnso = true; 
                pthread_mutex_unlock(&mutex_actuators);   
            }
            else if ( (room[file_rules[i].id].sound <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<="))) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"WARNSO");
                actuators[file_rules[i].id].warnso = false;
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra da Humidade
        else if (!strcmp(file_rules[i].sensor,"HUM")) {
            if ( (room[file_rules[i].id].humidity <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"DEHUM");
                actuators[file_rules[i].id].dehum = false;
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].humidity > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"DEHUM");
                actuators[file_rules[i].id].dehum = true; 
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra do Movimento
        else if (!strcmp(file_rules[i].sensor,"MOV")) {
            if ( (room[file_rules[i].id].movement <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"WARNM");
                actuators[file_rules[i].id].warnm = false; 
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].movement > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"WARNM");
                actuators[file_rules[i].id].warnm = true; 
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra da Luz
        else if (!strcmp(file_rules[i].sensor,"LUX")) {
            if ( (room[file_rules[i].id].light <= file_rules[i].valors)&& (!strcmp(file_rules[i].operator,"<="))) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"LAMP");
                actuators[file_rules[i].id].lamp = true;
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].light > file_rules[i].valors)&& (!strcmp(file_rules[i].operator,">"))) {    
                pthread_mutex_lock(&mutex_actuators);                   
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"LAMP");
                actuators[file_rules[i].id].lamp = false;
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra da potencia
        else if (!strcmp(file_rules[i].sensor,"POT")) {
            if ( (room[file_rules[i].id].power <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"POTD");
                actuators[file_rules[i].id].potd = false;  
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].power > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"POTD");
                actuators[file_rules[i].id].potd = true;  
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
        //Regra do fumo
        else if (!strcmp(file_rules[i].sensor,"SMOKE")) {
            if ( (room[file_rules[i].id].smoke <= file_rules[i].valors) && (!strcmp(file_rules[i].operator,"<=")) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"WARNSM");
                actuators[file_rules[i].id].warsm = false; 
                pthread_mutex_unlock(&mutex_actuators);
            }
            else if ( (room[file_rules[i].id].smoke > file_rules[i].valors) && (!strcmp(file_rules[i].operator,">")) ) {
                pthread_mutex_lock(&mutex_actuators);
                actuators[file_rules[i].id].id = file_rules[i].id;
                strcpy(actuators[file_rules[i].id].name,"WARNSM");
                actuators[file_rules[i].id].warsm = true; 
                pthread_mutex_unlock(&mutex_actuators);
            }
        }
    }
    pthread_exit(NULL);
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

    if (actuators[4].dehum && (adicional==1 || adicional==2)) { C23=CASTANHO;}
    else {C23=BRANCO;}

    if (actuators[2].dehum && adicional==2) { C24=CASTANHO;}
    else {C24=BRANCO;}
    if( actuators[2].warnm && adicional==2) {C25=VERDE;}
    else  {C25=BRANCO;}
    pthread_mutex_unlock(&mutex_actuators);

    printf("[%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s]\n",C1,C3,C8,C10,C19,C2,C23,C9,C11,C20,C24,C12,C13,C14,C25,C4,C6,C15,C17,C21,C5,C7,C16,C18,C22);

    pthread_exit(NULL);
}
