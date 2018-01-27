#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

int main( int argc, char** argv ) {
	msg r, t;
	init( HOST, PORT );

	if ( recv_message( &r ) < 0 ) {
		perror( "Receive message" );
		return -1;
	}
	printf( "[%s] Got msg with payload: %s\n", argv[0], r.payload );


	unsigned short crc = crc16_ccitt( r.payload, r.len );
	sprintf( t.payload, "CRC(%s)=0x%04X", r.payload, crc );
	t.len = strlen( t.payload );
	send_message( &t );

	int countTimeouts = 0;
	int expectedSequenceNumber = 0;
	msg *receive_message;
	MiniKermitFrame confirmationFrame;
	msg packedFrame;
	msg lastSentPacket;
	unsigned short receivedFrameCRC;

	/*
	 * Aceasta secventa este pentru primirea pachetului de tip send init in
	 * caz de timeout.
	 */
	while ( countTimeouts < 3 ) {
		receive_message = receive_message_timeout( TIME * 1000 );
		/*
		 * Daca avem timeout la primirea pachetului send init, atunci trebuie
		 * sa asteptam de inca doua ori retransmiterea lui. Deci in total
		 * pachetul send init poate fi transmis de maxim 3 ori.
		 */
		if ( receive_message == NULL ) {
			perror( "Receive message" );
			printf( "TIMEOUT\n" );
			countTimeouts++;

		}
		else
			break;

	}

	if ( countTimeouts == 3 ) {
		printf( "[%s] Receiverul isi intrerupe executia, deoarece pachetul nu"
		        " a fost primit dupa 3 timeouturi!\n", argv[0] );
		exit( -1 );
	}
	else
		printf( "[%s] Receiverul a primit pachetul si isi continua executia!\n",
		        argv[0] );

	/*
	 * Receiverul ruleaza atata timp cat se mentine transmisia.
	 */
	while ( receive_message->payload[3] != EOT ) {
		countTimeouts = 0;


		while ( countTimeouts < 3 ) {
			receive_message = receive_message_timeout( TIME * 1000 );
			if ( receive_message == NULL ) {
				perror( "Receive message" );
				printf( "[%s] TIMEOUT2\n", argv[0] );
				countTimeouts++;
				send_message( &lastSentPacket );
			}
			else
				break;
		}

		if ( countTimeouts == 3 ) {
			printf( "[%s] Receiverul isi intrerupe executia, deoarece pachetul nu"
			        " a fost primit dupa 3 timeouturi!\n", argv[0] );
			return 0;
		}
		else
			printf( "[%s] Receiverul a primit pachetul si isi continua executia!\n",
			        argv[0] );


		printf( "[%s] expectedSequenceNumber: %d -- received %d\n", argv[0],
		        expectedSequenceNumber,
		        receive_message->payload[2] );
		printf( "[%s] Got packet: %c with sequence number %d\n", argv[0],
		        receive_message->payload[3],
		        receive_message->payload[2] );

		/*
		 * Si in cazul receiverului trebuie verificat numarul de secventa primit.
		 * Observam ca pachetele primite de receiver au mereu numar de secventa
		 * par, deci putem initializa un contor cu 0 pentru a indica primirea
		 * pachetului de tip send init. Apoi, vom creste acest contor din 2 in 2
		 * si vom verifica daca pachetul primit are numarul de secventa asteptat.
		 * Daca nu, atunci retrimitem ultima confirmare(ACK sau NAK) si dam
		 * continue, pentru a sari peste procesarile ulterioare si a reprimi
		 * pachetul corespunzator.
		 */
		if ( expectedSequenceNumber != receive_message->payload[2] ) {
			send_message( &lastSentPacket );
			continue;
		}



		/*
		 * Campul payload al mesajului contine doar informatiile utile. Dintre
		 * acestea pentru calcularea crc nu avem nevoie de check si mark, deci
		 * ignoram ultimii 3 octeti.
		 */
		crc = crc16_ccitt( receive_message->payload, receive_message->len - 3 );

		/*
		 * Ca sa nu irosim memorie despachetand intreaga structura vom extrage din
		 * payload doar datele necesare cum ar fi numarul de secventa, campul de
		 * date si check.
		 */
		memcpy( &receivedFrameCRC, &receive_message->payload[receive_message->len - 3],
		        2 );

		if ( crc == receivedFrameCRC ) {
			printf( "Received CRC: %04X\n", receivedFrameCRC );
			printf( "Recalculated CRC: %04X\n", crc );
			printf( "packet received correctly!\n" );
			receive_message->payload[2] = ( receive_message->payload[2] + 1 ) % 64;

			/*
			 * Pentru pachetul de tip send init, cadrul de confirmare trebuie sa
			 * contina in campul de date, campul de date al pachetului primit.
			 */
			if ( receive_message->payload[3] == SEND_INIT )
				confirmationFrame = initializeFrame( ACK, "send", receive_message->payload[2],
				                                     11 );
			else
				confirmationFrame = initializeFrame( ACK, NULL, receive_message->payload[2],
				                                     0 );
				                                     
			packedFrame = packFrame( confirmationFrame );
			char filename[50];
			int file_descriptor_destination;
			switch (	receive_message->payload[3]	) {
				case SEND_INIT:
					printf( "Pachetul de tip send init a fost primit cu succes!\n" );
					break;

				case FILE_HEADER:

					strcpy( filename, "recv_" );
					memcpy( filename + 5, &receive_message->payload[4], receive_message->len - 7 );

					file_descriptor_destination = open( filename, O_WRONLY | O_CREAT, 0700 );

					if ( file_descriptor_destination < 0 ) {
						printf( "Eroare la crearea fisierului!\n" );
						exit( 1 );
					}
					else
						printf( "[%s] Crearea fisierului %s s-a realizat cu succes!\n", argv[0],
						        filename );

					break;

				case DATA:
					printf( "[%s] Se copiaza date in fisierul de iesire\n", argv[0] );
					write( file_descriptor_destination, &receive_message->payload[4],
					       receive_message->len - 7 );
					break;

				case EOFZ:
					printf( "[%s] Transmiterea fisierului s-a incheiat cu succes!\n", argv[0] );
					close( file_descriptor_destination );
					break;

				case EOT:
					printf( "[%s] Se incheie transmisia!\n", argv[0] );
					break;
			}
		}
		else {
			printf( "Received CRC: %04X\n", receivedFrameCRC );
			printf( "Recalculated CRC: %04X\n", crc );
			printf( "packet received corrupted!\n" );
			receive_message->payload[2] = ( receive_message->payload[2] + 1 ) % 64;
			confirmationFrame = initializeFrame( NAK, NULL, receive_message->payload[2],
			                                     0 );
			packedFrame = packFrame( confirmationFrame );
		}

		send_message( &packedFrame );
		/*
		 * Trebuie sa tinem evidenta ultimului pachet de confirmare trimis in
		 * cazul in care va fi necesara retransmisia.
		 */
		lastSentPacket = packedFrame;
		expectedSequenceNumber = ( expectedSequenceNumber + 2 ) % 64;

	}
	return 0;
}
