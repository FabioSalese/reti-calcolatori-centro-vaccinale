#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>      // Libreria che contiene le definizioni per le operazioni del database di rete.
#include <sys/types.h>  // Libreria che contiene le definizioni dei tipi di dati utilizzati nei sistemi Unix.
#include <sys/socket.h> // Libreria che contiene le definizioni dei socket.
#include <arpa/inet.h> // Libreria che contiene le definizioni per le operazioni Internet.
#include <unistd.h>   // Libreria che contiene le definizioni di funzioni e costanti standard del sistema Unix.
#include <errno.h>      // Libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <string.h>

#define DIM_MAX 1024    // Dimensione massima del buffer.
#define DIM_ACK 64      // Dimensione della risposta di conferma.
#define DIM_BENVENUTO 108 //Dimensione del messaggio di benvenuto dal ServerG
#define APP_ACK 39       //Dimensione dell'ACK ricevuto dall'AppVerifica
#define DIM_ID 11 // Dimensione del codice di tessera sanitaria (10 byte + 1 byte del carattere terminatore).


// Si leggono esattamente count byte iterando opportunamente le letture e anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buffer, size_t count) {
    size_t n_left;    // Numero di byte rimanenti da leggere.
    ssize_t n_read;   // Numero di byte letti
    n_left = count;   // Inizializza il numero di byte rimanenti da leggere.

    while (n_left > 0) {   // Ripeti finché non ci sono byte rimanenti da leggere.
        if ((n_read = read(fd, buffer, n_left)) < 0) {   // Legge dal file descriptor fino a n_left byte o fino alla chiusura della connessione.
            if (errno == EINTR) {
                continue;    // Se viene interrotta da una System Call che interrompe ripeti il ciclo.
            } else {
                exit(n_read);    // Altrimenti esce con il valore di errore n_read.
            }
        } else if (n_read == 0) {
            break;    // Se sono finiti i byte da leggere, esce dal ciclo.
        }
        n_left -= n_read;   // Aggiorna il numero di byte rimanenti da leggere.
        buffer += n_read;   // Sposta il puntatore all'inizio del buffer in modo che possa leggere il prossimo pezzo di dati.
    }
    buffer = 0;    // Imposta il buffer a zero.
    return n_left;   // Restituisce il numero di byte rimanenti da leggere.
}


// Scrive esattamente count byte s iterando opportunamente le scritture. Scrive anche se viene interrotta da una System Call.
ssize_t full_write(int fd, const void *buffer, size_t count) {
    size_t n_left;      // Numero di byte mancanti da scrivere
    ssize_t n_written;  // Numero di byte scritti
    n_left = count;
    while (n_left > 0) {  // Ripeti finchè non ci sono byte rimasti da scrivere
        if ((n_written = write(fd, buffer, n_left)) < 0) {  // Se la scrittura fallisce
            if (errno == EINTR) continue;  // Se la System Call viene interrotta ripeti il ciclo
            else exit(n_written);  // Altrimenti esci con un errore
        }
        n_left -= n_written;  // Aggiorna il numero di byte rimasti da scrivere
        buffer += n_written;  // Sposta il puntatore del buffer di scrittura in avanti
    }
    buffer = 0;
    return n_left;
}

int main() {
    int socket_fd, n;
    struct sockaddr_in server_addr;
    char start_bit = '0', buffer[DIM_MAX], id[DIM_ID];

   // Crea il descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Errore nella creazione del socket");
        exit(EXIT_FAILURE);
    }


// Inizializza la struttura dell'indirizzo del server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1026);

   // Converti l'indirizzo IP in formato dotted in un indirizzo di rete in network order.
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) == -1) {
        perror("Errore nella conversione dell'indirizzo IP");
        exit(EXIT_FAILURE);
    }

    // Effettua la connessione al server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Errore durante la connessione al server");
        exit(EXIT_FAILURE);
    }

  // Invia un segnale di inizio al ServerG per informarlo che la comunicazione deve avvenire con l'AppVerifica
    if (full_write(socket_fd, &start_bit, sizeof(char)) == -1) {
        perror("Errore nell'invio del segnale di inizio");
        exit(EXIT_FAILURE);
    }

   // Riceve il messaggio di benvenuto dal ServerG
    if (full_read(socket_fd, buffer, DIM_BENVENUTO) == -1) {
        perror("Errore nella ricezione del messaggio di benvenuto");
        exit(EXIT_FAILURE);
    }
    printf("%s\n\n", buffer);

    // Inserimento del codice della tessera sanitaria
    while (1) {
        printf("Inserisci il codice della tessera sanitaria. (Massimo 10 caratteri): ");
        if (fgets(id, DIM_MAX, stdin) == NULL) {
            perror("Errore nella lettura dell'input utente");
            exit(EXIT_FAILURE);
        }


        if (strlen(id) != DIM_ID) {
            printf("Il codice della tessera sanitaria deve essere di 10 caratteri. Riprovare\n\n");
        } else {
            id[DIM_ID - 1] = 0;
            break;

        }
    }

    // Invia il numero di tessera sanitaria
    if (full_write(socket_fd, id, DIM_ID) == -1) {
        perror("Errore nell'invio del numero di tessera sanitaria");
        exit(EXIT_FAILURE);
    }

    // Ricezione ack
    if (full_read(socket_fd, buffer, DIM_ACK) == -1) {
        perror("Errore nella ricezione dell'ack");
        exit(EXIT_FAILURE);
    }
    printf("\n%s\n\n", buffer);

    printf("Convalida in corso! Per favore attendere...\n\n");

    // Facciamo attendere 2 secondi per completare l'operazione di verifica
    sleep(2);

    // Riceve esito scansione Green Pass dal ServerG
    if (full_read(socket_fd, buffer, APP_ACK) == -1) {
        perror("Errore nella ricezione dell'esito scansione");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", buffer);

    close(socket_fd);

    exit(EXIT_SUCCESS);
}


