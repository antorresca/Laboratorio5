#include<xc.h> //Libreria para PIC
#include<stdio.h> //Libreria adicional para manejo de decimales
#include "LibLCDXC8.h" //Libreria para uso de LCD

#pragma config FOSC=INTOSC_EC //Sentencia para usar oscilador externo
#pragma config WDT=OFF //Apagar el perro guardian
#pragma config PBADEN=OFF 
#pragma LVP=OFF

#define _XTAL_FREQ 8000000 //Frecuencia de reloj
#define DATA_DIR TRISC2
#define DATA_IN RC2
#define DATA_OUT LATC2

unsigned char Temp, Hum, Che;
unsigned char TempEEPROM;
unsigned char TempEEPROM;//Para el bonus
unsigned int IniFlag = 0;



//Prototipos de las funciones
void LeerHT11(void);
unsigned char LeerByte(void);
unsigned char LeerBit(void);
unsigned char Check();
void Transmitir(unsigned char);
unsigned char Recibir(void);
void TransmitirDatos(unsigned int Ent1, unsigned int Ent2);
void ColorRGB(unsigned int val);
unsigned int Conversion(unsigned char);
void GuardarDatoEnEEPROM(unsigned int dir, unsigned char Dato);
unsigned char leerDatoEnEEPROM(unsigned int dir);

//void LedPot(void);

void main(void) {

    //CONFIGURACION PARA EL RS232
    OSCCON = 0b01110000; //Establece el reloj interno en 8Mhz
    __delay_ms(1);
    DATA_OUT = 0;
    TXSTA = 0b00100000; //Configuración del transmisor, habilitación del transmisor y modo asincrónico, bajas velocidades
    RCSTA = 0b10010000; //Configuración del receptor, habilitación del modulo EUSART, se habilita el receptor
    BAUDCON = 0b00000000; //Configuracion del modulo adc, no inversion logica, divisor de frecuencia 8bits, modo bajo consumo desactivado,
    //autodeteccion de velocidad off.
    SPBRG = 12; //Valor para la vel de transmisión de datos, revisar formula -> SPBRG = 8M/(64*9600)-1
    //ConfigADC
    ADCON0 = 0b00000001;
    ADCON1 = 13;
    ADCON2 = 0b10001000; //Justificacion a derecha, adquisicion instantanea
    //Fin config ADC
    //
    //CONFIGURACION DE PUERTOS I/O
    TRISB = 0; //Colocar puerto B como salida
    TRISD = 0;//Colocar puerto D como salida
    TRISA = 0b00000001; //Colocar pines A00 como entrada digital para ADC
    TRISC = 0b11000111; //Colocar Pines C0 y C1 como entrada (seleccion de temperatura) C2 entrada Para la lectura de datos RC6 como entrada para activar TX, para escritura RC7
    RBPU = 0; //Activar resistencias pull up
    
    TempEEPROM = leerDatoEnEEPROM(0);
    
    InicializaLCD(); //Funcion para configuracion inicial del LCD
    ConfiguraLCD(4);
    //Timer0 interrupcion
    T0CON=0b10000011;//Habilita timer0, 16 bits de resolucion, reloj interno
    TMR0IF=0;// apaga bandera
    TMR0=3036; // valor pre carga
    TMR0IE=1; //Habilita la interrupcion 
    GIE=1; //habilita interrupciones globales
    TMR0ON=1;//Habilita la interrupcion Timer0, primer bit de T0CON
    //Fin de configuracion para Timer0
   
    BorraLCD(); //Limpiar el LCD

    if (TempEEPROM != 0xFF) {
        MensajeLCD_Word("Ultima temp:");
        DireccionaLCD(192);
        EscribeLCD_c(TempEEPROM / 10 + 48);
        EscribeLCD_c(TempEEPROM % 10 + 48);
        EscribeLCD_c('C');
        __delay_ms(2000);
        BorraLCD();
    }
    
    MensajeLCD_Word("Iniciando"); //Escribir mensaje de bienvenida
    __delay_ms(500); //Retraso para evitar errores
    EscribeLCD_c(46);
    __delay_ms(500);
    EscribeLCD_c(46);
    __delay_ms(500);
    EscribeLCD_c(46);
    __delay_ms(500);
    BorraLCD();
    
    
    while (1) {
        __delay_ms(500);
        //LATD0=1; LED?
        __delay_ms(500);
        //LATD0=0;
        LeerHT11();
        IniFlag = 1;
        GuardarDatoEnEEPROM(0, Temp);
        ColorRGB(Temp); //Importante asignar primero el color porque la transmision modifica Temp y Hum
        
        
        TransmitirDatos(RC0, RC1);
        
        Conversion(0);
        RD3 = (ADRES <= 511) ? 0 : 1; //2.5*(2^10-1)/5 
    }
}

void LeerHT11(void) {
    unsigned char i, contr = 0;
    //Se le dan dos pulsos al sensor para que haga la medición
    DATA_DIR = 0;
    __delay_ms(18);
    //Pulso 1
    DATA_DIR = 1;
    while (DATA_IN == 1);
    __delay_us(40);
    if (DATA_IN == 0) contr++;
    __delay_us(80);
    //Pulso 2 
    if (DATA_IN == 1) contr++;
    while (DATA_IN == 1); //Termina cuando el sensor toma control de la línea
    //Recepción de datos
    Hum = LeerByte();
    LeerByte();
    Temp = LeerByte();
    LeerByte();
    Che = LeerByte();
}

unsigned char LeerByte(void) {
    unsigned char res = 0, i;
    for (i = 8; i > 0; i--) {
        res = (res << 1) | LeerBit(); //Va moviendo los digitos del byte a la derecha y añadiendo los valores leídos
    } //Comienza 00000000, lee 1, entonces 0000001, lee 0, entonces 00000010, lee 1, entonces 00000101, hasta llenar el byte
    return res;
}

unsigned char LeerBit(void) {
    unsigned char res = 0;
    while (DATA_IN == 0);
    __delay_us(13);
    if (DATA_IN == 1) res = 0; //Si el pulso dura menos de 30 us el bit es 0
    __delay_us(22);
    if (DATA_IN == 1) { // Sino, el bit es 1
        res = 1;
        while (DATA_IN == 1);
    }
    return res;
}

unsigned char Check(void) {
    unsigned char res = 0, aux;
    aux = Temp + Hum;
    if (aux == Che) res = 1;
    return res;
}

void Transmitir(unsigned char BufferT) {
    while (TRMT == 0);
    TXREG = BufferT;
}

unsigned char Recibir(void){
    while(RCIF==0);
    return RCREG;
}

void TransmitirDatos(unsigned int Ent1, unsigned int Ent2) {
    unsigned int n = Ent1 * 10 + Ent2, TempC = Temp, HumC = Hum;
    unsigned int Simb = 67;
    BorraLCD();
    switch (n) {
        case 00://Celsius
            TempC = Temp;
            Simb = 67; //C
            break;
        case 01://Kelvin
            TempC = Temp + 273;
            Simb = 75; //K
            break;
        case 10://Rankine
            TempC = Temp * 9 / 5 + 492;
            Simb = 82; //R
            break;
        case 11://Fahrenheit
            TempC = Temp * 9 / 5 + 32;
            Simb = 70; //F
            break;
    }
    Transmitir('T');
    Transmitir('e');
    Transmitir('m');
    Transmitir('p');
    Transmitir(':');
    Transmitir(' ');

    MensajeLCD_Word("Temp:");
    if (TempC / 100 > 0) {
        Transmitir(TempC / 100 + 48);
        EscribeLCD_c(TempC / 100 + 48);
        TempC = TempC % 100;
    }
    Transmitir(TempC / 10 + 48);
    Transmitir(TempC % 10 + 48);
    Transmitir(167);
    Transmitir(Simb);
    Transmitir(' ');
    Transmitir('\n');
    Transmitir('H');
    Transmitir('u');
    Transmitir('m');
    Transmitir(':');
    Transmitir(' ');
    Transmitir(Hum / 10 + 48);
    Transmitir(Hum % 10 + 48);
    Transmitir(' ');
    Transmitir('%');
    Transmitir('\n');
    Transmitir(' ');


    //Transmision LCD


    EscribeLCD_c(TempC / 10 + 48);
    EscribeLCD_c(TempC % 10 + 48);
    EscribeLCD_c(Simb);
    DireccionaLCD(192);
    MensajeLCD_Word("Hum:");
    EscribeLCD_c(Hum / 10 + 48);
    EscribeLCD_c(Hum % 10 + 48);
    EscribeLCD_c('%');

}

void ColorRGB(unsigned int val) {
    if (val < 22) {
        RD0 = 0;
        RD1 = 0;
        RD2 = 0;
    } else if (val >= 22 && val < 25) {
        RD0 = 1;
        RD1 = 0;
        RD2 = 1;
    } else if (val >= 25 && val < 28) {
        RD0 = 0;
        RD1 = 0;
        RD2 = 1;
    } else if (val >= 28 && val < 31) {
        RD0 = 0;
        RD1 = 1;
        RD2 = 1;
    } else if (val >= 31 && val < 34) {
        RD0 = 0;
        RD1 = 1;
        RD2 = 0;
    } else if (val >= 34 && val < 37) {
        RD0 = 1;
        RD1 = 1;
        RD2 = 0;
    } else if (val >= 37 && val < 40) {
        RD0 = 1;
        RD1 = 0;
        RD2 = 0;
    } else if (val >= 40) {
        RD0 = 1;
        RD1 = 1;
        RD2 = 1;
    }
}

unsigned int Conversion(unsigned char canal) {
    ADCON0 = 0b00000001 | (canal << 2);
    GO = 1; //bsf ADCON0,1
    while (GO == 1);
    return ADRES;
}

void GuardarDatoEnEEPROM(unsigned int dir, unsigned char dato) {
    EEADR = dir;
    EEDATA = dato;
    //Configuracion EECON1
    EEPGD = 0; //Selecciona la memoria EPROM
    CFGS = 0; //Accede a la configuracion de EEPROM
    WREN = 1; //Habilita la escritura

    GIE = 0; //Desabilita interrupciones, recomendacion de la datasheet (INTCON)
    //Secuencia obligatoria del puerto EECON2
    EECON2 = 0x55;
    EECON2 = 0xAA;
    WR = 1; //Permite la escritura
    GIE = 1; //Rehabilita interrupciones,
    while (!EEIF);
    EEIF = 0; //Apaga la bandera
    WREN = 0; //Desabilita la escritura

}

unsigned char leerDatoEnEEPROM(unsigned int dir) {
    EEADR = dir; //Establece la direccion a leer
    EEPGD = 0; //Selecciona la memoria EPROM
    CFGS = 0; //Accede a la configuracion de EEPROM
    RD = 1; //Activa lectura

    return EEDATA;
}


void __interrupt() ISR(void){
    if(TMR0IF==1){
        TMR0IF=0;
        RD4 = !RD4;
        TMR0 = 3036;//Precarga 2^n - Tsobreflujo*Fbus_Timer0/PreScaler
        //Tuvo que usarse una resolucion de 16 bits y un PS de  para lograr el valor deseado
    }
    if(RCIF && IniFlag == 1){
        switch (Recibir()){
                case 'C':
                    TransmitirDatos(0, 0);
                    break;
                case 'K':
                    TransmitirDatos(0, 1);
                    break;
                case 'R':
                    TransmitirDatos(1, 0);
                    break;
                case 'F':
                    TransmitirDatos(1, 1);
                    break;
                default:
                    break;
        }
       
        __delay_ms(1000);
        
    }
            
}
