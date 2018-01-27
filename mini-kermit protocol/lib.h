#ifndef LIB
#define LIB

#define MAXL 250 //bytes
#define TIME 5 //secunde
#include <string.h>
/*
 *Aici am definit constante ce indica tipul pachetelor.
 */
#define SEND_INIT 'S' 
#define FILE_HEADER 'F'  
#define DATA 'D' 
#define EOFZ 'Z' 
#define EOT 'B' 
#define ACK 'Y' 
#define NAK 'N' 
#define ERROR 'E' 

typedef struct {
    int len;
    char payload[1400];
} msg;

/*
 * Structura pachetului mini-kermit.
 */
typedef struct {
	char soh; //start of header; marcheaza inceputul unui cadru;
	/*
	 * Folosim unsigned char, deoarece valoarea maxima a lui len este 255.
	 */
	unsigned char len; //length; lungimea pachetului - 2(cei 2 biti folositi pentru soh 
			  //si len);
	char seq; //numarul de secventa, modulo 64;
	char type; //tipul pachetului;
	char data[MAXL]; //datele din pachet
	short check; //suma de control, calculata pe toate campurile in afara de 				  
				 //campul mark
	char mark; //end of block marker; marcheaza sfarsitul cadrului;
} MiniKermitFrame;

/*
 * Structura campului de date al pachetului de tip SEND_INIT.
 */
typedef struct {
	unsigned char maxl; //lungimea maxima a pachetelor receptionate;
	char time; //durata de timeout pentru un pachet, in secunde;
	char npad; //numarul de caractere de padding emise inainte de fiecare pachet;
	char padc; //caracterul folosit pentru padding;
	char eol; //caracterul folosit in campul MARK. Implicit este caracterul 
			  //0x0D (CR);
	char qctl;
	char qbin;
	char chkt;
	char rept;
	char capa;
	char r;
} SendInitData;



void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

SendInitData initializeSendInitDataFrame() {
    SendInitData data;
  
    data.maxl = MAXL;
    data.time = TIME; 
    data.npad = 0x00;  
    data.padc = 0x00; 
    data.eol = 0x0D; 
    data.qctl = 0x00;
    data.qbin = 0x00;
    data.chkt = 0x00;
    data.rept = 0x00;
    data.capa = 0x00;
    data.r = 0x00;

    return data;
}

MiniKermitFrame initializeFrame(char type, char data[], char seq,int dataSize) {
	MiniKermitFrame frame;
	SendInitData sendInitData = initializeSendInitDataFrame();
	
	/*
	 * Pachetele au o serie de campuri comune precum soh, seq, check (deoarece
	 * se calculeaza in functie de len, mark.
	 */
	frame.soh = 0x01;
	frame.seq = seq;

	/*
	 * Campurile care variaza in functie de tipul de pachet sunt data, type si
	 * len.
	 */	
	switch(type) {
		case SEND_INIT:
			/*
    		 * campurile seq, type, mark au cate un octet;
     		 * campul check are 2 octeti;
    		 * campul data al unui pachet de tip send init are 11 octeti;
     		 * in total 16;
     		*/
    		frame.len = 16; 
		    frame.type = type;
		    memcpy(frame.data, &sendInitData, sizeof(sendInitData));
			break;	
			
		case FILE_HEADER:
			frame.len = 5 + dataSize;
			frame.type = type;
			memcpy(frame.data,data,dataSize);			
			break;
			
		case DATA:
			frame.len = 5 + dataSize;
			frame.type = type;
			memcpy(frame.data,data,dataSize);	
			break;
		
		case ACK:
			/* 
			 * Pachetul de tip ACK pentru confirmarea primirii unui pachet de tip
			 * send init contine datele acestuia in campul sau de date, altfel il
			 * are vid.
			 */
			if( data == NULL ) 
				frame.len = 5;
			else {
				memcpy(frame.data, &sendInitData, sizeof(sendInitData));		
				frame.len = 16;
			}	
			frame.type = type;		
			break;
			
		case EOFZ:		 	
		case EOT:
		case NAK:
			frame.len = 5;
		 	frame.type = type;
			break;    
		
		default:
			printf("Invalid frame type!");
	}
	
	/*
     * Suma de control se calculeaza pentru toate campurile structurii ce
     * modeleaza un pachet mini kermit cu exceptia campului mark care are 
     * dimensiunea un octet. Deci vom apela functia ce calculeaza suma de control
     * cu adresa din memorie unde este memorat cadrul, iar dimensiunea va fi
     * len - 1, deoarece len indica dimensiunea efectiv folosita in cadrul
     * structurii. Noi trebuie sa calculam crc pentru primele 4 campuri ale 
     * structurii plus partea utila din data, deci ignoram campurile check si 
     * mark care impreuna au 3 octeti.
     */
	frame.check = crc16_ccitt(&frame, frame.len - 1);
	frame.mark =  sendInitData.eol;
	
	return frame;
}

msg packFrame(MiniKermitFrame frame) {
	msg packedFrame;
	
    /*
     * Campul data al cadrului are o capacitate maxima de 250 de bytes, insa
     * de multe ori este posibil sa folosim mult mai putin de atat. Din acest
     * motiv, ne dorim ca sa transmitem la receiver doar continutul util al
     * campului data. Pentru aceasta vom memora in campul payload al mesajului
     * ce va fi transmis continutul util al pachetului.
     *
     * Pentru a putea realiza operatii pe zone de memorie cu mai multa libertate
     * vom muta continutul cadrului intr-un buffer alocat static.
     */
    char frame_buffer[258];
    /*
     * Folosesc functia memmove pentru a muta campurile check si mark de la finalul
     * structurii, imediat dupa ultima informatie utila din campul data. Insa,
     * dimensiunea structurii a ramas aceeasi. De aceea, voi copia cu ajutorul
     * functiei memcpy exact datele utile din cadrul modificat anterior, iar
     * aceste informatii le voi pune in campul payload al mesajului.
     */
    memcpy(frame_buffer,&frame,sizeof(frame));
    memmove(frame_buffer + frame.len - 1, frame_buffer + MAXL + 4, 3);
    char usefulData[frame.len + 2];
    memcpy(usefulData,frame_buffer,frame.len + 2);
    
    memcpy(packedFrame.payload,usefulData,frame.len + 2);
    /*
     * Lungimea utila este frame.len + numarul de bytes care nu sunt luati in
     * considerare (soh si len) adica 2.
     */
    packedFrame.len = frame.len + 2;
    
    return packedFrame;
}

#endif
