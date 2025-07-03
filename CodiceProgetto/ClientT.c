#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // Libreria che contiene le definizioni di funzioni e costanti standard del sistema Unix.
#include <sys/types.h>   // Libreria che contiene le definizioni dei tipi di dati utilizzati nei sistemi Unix.
#include <sys/socket.h> // Libreria che contiene le definizioni dei socket.
#include <arpa/inet.h>  // Libreria che contiene le definizioni per le operazioni Internet.
#include <errno.h>      // Libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <string.h>
#include <netdb.h>      // Libreria che contiene le definizioni per le operazioni del database di rete.

#define DIM_MAX 1024   // Dimensione massima del buffer.
#define DIM_ID 11         // Dimensione del codice di tessera sanitaria (10 byte + 1 byte del carattere terminatore).
#define ACK_SIZE 61         // Dimensione della risposta di conferma.
#define DIM_ACK 39 //Dimensione degli ACK

// Definizione di una struttura dati chiamata "REPORT".
// La struttura contiene due campi: "ID" e "report". Questa struttura viene utilizzata per rappresentare il pacchetto contenente il numero di tessera sanitaria di un green pass ed il suo referto di validità.
typedef struct  {
    char ID[DIM_ID];
    char report;
} REPORT;

                                                 // Si leggono esattamente count byte iterando opportunamente le letture e anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buffer, size_t count) {
    size_t n_left;   // Numero di byte rimanenti da leggere.
    ssize_t n_read;  // Numero di byte letti
    n_left = count;  // Inizializza il numero di byte rimanenti da leggere.
    while (n_left > 0) {  // Ripeti finché non ci sono byte rimanenti da leggere.
        if ((n_read = read(fd, buffer, n_left)) < 0) {  // Legge dal file descriptor fino a n_left byte o fino alla chiusura della connessione.
            if (errno == EINTR) continue;  // Se viene interrotta da una System Call che interrompe ripeti il ciclo.
            else exit(n_read);  // Altrimenti esce con il valore di errore n_read.
        } else if (n_read == 0) break;  // Se sono finiti i byte da leggere, esce dal ciclo.
            n_left -= n_read;  // Aggiorna il numero di byte rimanenti da leggere.
            buffer += n_read;  // Sposta il puntatore all'inizio del buffer in modo che possa leggere il prossimo pezzo di dati.
        }
        buffer = 0;  // Imposta il buffer a zero.
        return n_left;  // Restituisce il numero di byte rimanenti da leggere.
        }


// Scrive esattamente count byte s iterando opportunamente le scritture. Scrive anche se viene interrotta da una System Call.
ssize_t full_write(int fd, const void *buffer, size_t count) {
    size_t n_left; // Numero di byte mancanti da scrivere
    ssize_t n_written; // Numero di byte scritti
    n_left = count;
    while (n_left > 0) { // Ripeti finchè non ci sono byte rimasti da scrivere
        if ((n_written = write(fd, buffer, n_left)) < 0) { // Se la scrittura fallisce
            if (errno == EINTR) continue; // Se la System Call viene interrotta ripeti il ciclo
            else exit(n_written); // Altrimenti esci con un errore
          }
            n_left -= n_written; // Aggiorna il numero di byte rimasti da scrivere
            buffer += n_written; // Sposta il puntatore del buffer di scrittura in avanti
         }
            buffer = 0;
            return n_left;
        }

int main(int argc, char **argv) {
    int socket_fd; // file descriptor del socket
    struct sockaddr_in server_addr; // struttura che contiene l'indirizzo del server a cui connettersi
    REPORT pacchetto; // struttura dei dati da inviare al server
    char bit_start, buffer[DIM_MAX];

    bit_start = '1'; // Inizializziamo il bit a 1 per indicare al server che la comunicazione deve avvenire con ServerG.

    // Creazione descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Errore durante la creazione del socket");
        exit(1);
    }

    // Impostiamo i parametri dell'indirizzo del server
    server_addr.sin_family = AF_INET; // utilizziamo gli indirizzi IP versione 4
    server_addr.sin_port = htons(1026); // numero di porta del server

    // Convertiamo l'indirizzo IP fornito in input come una stringa nel formato puntato-decimale in un indirizzo di rete network order.
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Errore durante la conversione dell'indirizzo IP del server");
        exit(1);
    }

    // Connessione col server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Errore durante la connessione con il server");
        exit(1);
    }

    // Invio di un bit valore "1" per informare il ServerG della comunicazione col ClienT
    if (full_write(socket_fd, &bit_start, sizeof(char)) < 0) {
        perror("Errore durante l'invio del bit di valore 1 al ServerG");
        exit(1);
    }


   // Stampiamo un messaggio per indicare all'utente cosa deve fare
printf("Inserire codice di tessera sanitaria e referto del tampone per invalidare o ripristinare Green Pass.\n");

// Inserimento del codice tessera sanitaria con controllo
while (1) {
    // Chiediamo all'utente di inserire il codice tessera sanitaria
    printf("Inserisci il codice tessera sanitaria (max 10 caratteri): ");

    // Leggiamo l'input dell'utente tramite fgets
    if (fgets(pacchetto.ID, DIM_MAX, stdin) == NULL) {
        // Se fgets restituisce un errore, stampiamo un messaggio di errore e usciamo dal programma
        perror("Errore nell'input dell'utente");
        exit(1);
    }

    // Controlliamo la lunghezza dell'input dell'utente
    if (strlen(pacchetto.ID) != DIM_ID) {
        // Se la lunghezza non è corretta, stampiamo un messaggio di errore e chiediamo all'utente di riprovare
        printf("Errore: il numero di caratteri del codice tessera sanitaria non è corretto.\nInserisci un codice con esattamente %d caratteri.\n", DIM_ID - 1);
    } else {
        // Altrimenti, inseriamo il terminatore al posto dell'invio inserito dall'utente tramite fgets
        pacchetto.ID[DIM_ID - 1] = 0;
        break;
    }
}

    while (1) {
printf("Inserisci 0 per invalidare il Green Pass o 1 per ripristinarlo: ");
scanf("%c", &pacchetto.report);
//Controllo sull'input dell'utente, se diverso da 0 o 1 richiede di ripetere l'operazione
if (pacchetto.report == '1' || pacchetto.report == '0') break;
printf("Errore: l'input inserito non è valido. Inserisci 0 o 1\n\n");
}

// Controllo input utente
if (pacchetto.report == '1') printf("\nAttendi. Stiamo inviando la richiesta per ripristinare il Green Pass inserito...\n");
else printf("\nAttendi. Stiamo inviando la richiesta per sospendere il Green Pass inserito...\n");

//Invia il pacchetto report al ServerG
if (full_write(socket_fd, &pacchetto, sizeof(REPORT)) < 0) {
perror("Errore full_write()");
exit(1);
}

//Riceve il messaggio di report dal ServerG
if (full_read(socket_fd, buffer, DIM_ACK) < 0) {
perror("Errore full_read()");
exit(1);
}

//Aggiunge un ritardo di 2 secondi per simulare un caricamento
sleep(2);

//Stampa il messaggio di report ricevuto dal ServerG
printf("%s\n", buffer);

exit(0);
}


