#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"
#include <stddef.h>

#define HOST "127.0.0.1"
#define PORT 10000

msg* send_message_procedure( msg packedFrame, char** argv ) {
	int countTimeouts = 0;
	send_message( &packedFrame );
	msg *receive_message;
	/*
	 * Putem retransmite de maxim 3 ori pachetul in caz de timeout.
	 */
	while ( countTimeouts < 3 ) {
		receive_message = receive_message_timeout( TIME * 1000 );

		if ( receive_message == NULL ) {
			printf( "[%s] Received timeout for packet acknowledgement!\n", argv[0] );
			countTimeouts++;
			send_message( &packedFrame );
		}
		else
			break;
	}

	/*
	 * Intrerupem transmisia daca nu am putut trimite pachetul send init de
	 * 3 ori.
	 */
	if ( countTimeouts == 3 )
		exit( 0 );

	/*
	 * Trebuie sa ne asiguram ca primim pachetul de confirmare corespunzator
	 * pachetului pe care l-am transmis, verificand numarul de secventa. Astfel,
	 * pachetul de confirmare va avea numarul de secventa al pachetului trimis
	 * + 1. In cazul in care pachetul primit are un numar de secventa gresit
	 * retrimitem pachetul si asteptam sa primim confirmarea corespunzatoare.
	 */
	while ( receive_message->payload[2] != packedFrame.payload[2] + 1 ) {
		send_message( &packedFrame );
		while ( countTimeouts < 3 ) {
			receive_message = receive_message_timeout( TIME * 1000 );

			if ( receive_message == NULL ) {
				printf( "[%s] Received timeout for packet acknowledgement!\n", argv[0] );
				countTimeouts++;
				send_message( &packedFrame );
			}
			else
				break;
		}
	}
	
	/*
	 * Intrerupem transmisia daca nu am putut trimite pachetul send init de
	 * 3 ori.
	 */
	if ( countTimeouts == 3 )
		exit( 0 );


	printf( "[%s] expectedSequenceNumber: %d -- received %d\n", argv[0],
	        packedFrame.payload[2] + 1,
	        receive_message->payload[2] );
	printf( "[%s] Received packet acknowledgement for (%c,%d)  : %c with "
	        "sequence number %d\n", argv[0], packedFrame.payload[3], packedFrame.payload[2],
	        receive_message->payload[3],
	        receive_message->payload[2] );

	/*
	 * Cat timp pachetul trimis nu a ajuns corect la destinatie, acesta trebuie
	 * retrimis, actualizand de fiecare data numarul de secventa.
	 */
	while ( receive_message->payload[3] == NAK ) {
		printf( "[%s] Last packet was corrupted!\n", argv[0] );

		packedFrame.payload[2] = ( receive_message->payload[2] + 1 ) % 64;
		send_message( &packedFrame );
		receive_message = receive_message_timeout( TIME * 1000 );
		/*
		 * Cat timp se genereaza timeout la primirea confirmarii nu putem trece
		 * mai departe.
		 */
		while ( receive_message == NULL && countTimeouts < 3 ) {
			printf( "[%s] Received timeout for packet acknowledgement!\n", argv[0] );
			countTimeouts++;
			send_message( &packedFrame );
			receive_message = receive_message_timeout( TIME * 1000 );
		}
		if ( countTimeouts == 3 )
			exit( 0 );
	}

	if ( receive_message->payload[3] == ACK )
		printf( "[%s] Last packet was received properly!\n", argv[0] );

	return receive_message;
}

int main( int argc, char** argv ) {
	msg t;


	init( HOST, PORT );


	sprintf( t.payload, "Hello World of PC" );
	t.len = strlen( t.payload );
	send_message( &t );


	msg *y = receive_message_timeout( 5000 );
	if ( y == NULL )
		perror( "receive error" );
	else
		printf( "[%s] Got reply with payload: %s\n", argv[0], y->payload );

	/*
	 * Trimitem pachetul de tip send init.
	 */
	MiniKermitFrame sendInitFrame = initializeFrame( SEND_INIT, NULL, 0, 11 );
	msg packedFrame = packFrame( sendInitFrame );
	msg* receive_message = send_message_procedure( packedFrame, argv );

	int fileCounter = 1;
	int file_descriptor_source;
	char buffer[MAXL];
	while ( fileCounter < argc ) {
		MiniKermitFrame fileHeaderFrame = initializeFrame( FILE_HEADER,
		                                  argv[fileCounter],
		                                  ( receive_message->payload[2] + 1 ) % 64, strlen( argv[fileCounter] ) );
		packedFrame = packFrame( fileHeaderFrame );
		receive_message = send_message_procedure( packedFrame, argv );

		int bytes_read = 0;
		file_descriptor_source = open( argv[fileCounter], O_RDONLY, 0700 );

		if ( file_descriptor_source < 0 ) {
			printf( "Eroare la deschiderea fisierului!\n" );
			exit( 1 );
		}
		else
			printf( "Deschiderea fisierului %s s-a realizat cu succes!\n",
			        argv[fileCounter] );

		while ( ( bytes_read = read( file_descriptor_source, buffer,
		                             MAXL ) ) > 0 ) {

			MiniKermitFrame dataFrame = initializeFrame( DATA, buffer,
			                            ( receive_message->payload[2] + 1 ) % 64, bytes_read );
			packedFrame = packFrame( dataFrame );
			receive_message = send_message_procedure( packedFrame, argv );

		}

		MiniKermitFrame eofFrame = initializeFrame( EOFZ, NULL,
		                           ( receive_message->payload[2] + 1 ) % 64, 0 );
		packedFrame = packFrame( eofFrame );
		receive_message = send_message_procedure( packedFrame, argv );
		fileCounter++;

	}

	MiniKermitFrame eotFrame = initializeFrame( EOT, NULL,
	                           ( receive_message->payload[2] + 1 ) % 64, 0 );
	packedFrame = packFrame( eotFrame );
	receive_message = send_message_procedure( packedFrame, argv );
	printf( "[%s] Transmisie incheiata!\n", argv[0] );

	return 0;
}



