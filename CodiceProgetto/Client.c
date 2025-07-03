#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   // Libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <netdb.h>           // Libreria che contiene le definizioni per le operazioni del database di rete.
#include <sys/types.h> // Libreria che contiene le definizioni dei tipi di dati utilizzati nei sistemi Unix.
#include <sys/socket.h> // Libreria che contiene le definizioni dei socket.
#include <arpa/inet.h>  // Libreria che contiene le definizioni per le operazioni Internet.
#include <errno.h>      // Libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <string.h>

#define DIM_MAX 1024   // Dimensione massima del buffer
#define DIM_ID 11      // Dimensione del codice di tessera sanitaria (10 byte + 1 byte del carattere terminatore).
#define DIM_ACK 61     // Dimensione della risposta di conferma.

//Questa è la struct del pacchetto che il centro vaccinale riceve dal client.
typedef struct {
    char name[DIM_MAX];
    char surname[DIM_MAX];
    char ID[DIM_ID];
} RICHIESTA_VACCINO;


//Procedura per la creazione del pacchetto che verrà inviato al Centro Vaccinale
RICHIESTA_VACCINO creazione_pacchetto() {
    char buffer[DIM_MAX];
    RICHIESTA_VACCINO crea_pacchetto;



    //Inserimento cognome 
    printf("Inserisci cognome: ");
    if (fgets(crea_pacchetto.surname, DIM_MAX, stdin) == NULL) {
        perror("fgets() error");
    }


    //Inserimento nome
    printf("Inserisci nome: ");
    if (fgets(crea_pacchetto.name, DIM_MAX, stdin) == NULL) {
        perror("fgets() error");
    }
    
   //La fgets inserisce l'invio come carattere, andiamo a sostituirlo col terminatore
    crea_pacchetto.surname[strlen(crea_pacchetto.surname) - 1] = 0;

   //La fgets inserisce l'invio come carattere, andiamo a sostituirlo col terminatore
    crea_pacchetto.name[strlen(crea_pacchetto.name) - 1] = 0;

    //Inserimento codice tessera sanitaria del client
    while (1) {
        printf("Inserire tessera sanitaria (Massimo 10 caratteri): ");
        if (fgets(crea_pacchetto.ID, DIM_MAX, stdin) == NULL) {
            perror("fgets() error");
            exit(1);
        }
        //Controllo sull'input del client
        if (strlen(crea_pacchetto.ID) != DIM_ID) printf("Errore... \nIl numero dei caratteri della tessera sanitaria non è corretto! Riprovare\n");
        else {
            //La fgets inserisce l'invio come carattere, andiamo a sostituirlo col terminatore
            crea_pacchetto.ID[DIM_ID - 1] = 0;
           break;
        }
    }
    return crea_pacchetto;
}



// Si leggono esattamente count byte iterando opportunamente le letture e anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buffer, size_t count) {
    size_t n_left;    // Numero di byte rimanenti da leggere.
    ssize_t n_read;   // Numero di byte letti.
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

int main(int argc, char **argv) {
    /* Inizializzazione di tre variabili: Isocket_fd sarà utilizzata per creare il socket, dim_benvenuto per memorizzare la dimensione del pacchetto di benvenuto inviato dal server, e dim_pacchetto per memorizzare la dimensione del pacchetto contenente la richiesta di vaccino.
     */
    int socket_fd, dim_benvenuto, dim_pacchetto;
    struct sockaddr_in server_addr; // indirizzo del server.
    RICHIESTA_VACCINO pacchetto; // pacchetto che verrà inviato al server
    char buffer[DIM_MAX]; // buffer per memorrizare le risposte del server
    char **alias; // utilizzata per memorizzare gli alias dell'host
    char *addr; // utilizzate per memorizzare indirizzo IP del server
    struct hostent *data; //struttura per utilizzare la gethostbyname e contenere le informazioni sull'host remoto

    if (argc != 2) {
        perror("usage: <host name>"); //perror: Crea un messaggio di errore che descrive l'ultimo errore verificatosi durante l'esecuzione di una chiamata di sistema o di una funzione di libreria.
        exit(1);
    }

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }


    server_addr.sin_family = AF_INET; // utilizzeremo  gli indirizzi IP versione 4
    server_addr.sin_port = htons(1024); // porta di rete

   //Trasformazione di un nome di dominio in un indirizzo IP.
    if ((data = gethostbyname(argv[1])) == NULL) {
        herror("Errore nella funzione gethostbyname()");
        exit(1);
    }
    alias = data -> h_addr_list;

   //Per convertire l'indirizzo in una stringa si utilizza inet_ntop
if ((addr = (char *)inet_ntop(data -> h_addrtype, *alias, buffer, sizeof(buffer))) < 0) {
    perror("Errore nella funzione inet_ntop()");
    exit(1);
}

//Trasformazione dell'indirizzo IP, inserito come una stringa nel formato puntato-decimale, in un indirizzo di rete nell'ordine network.
if (inet_pton(AF_INET, addr, &server_addr.sin_addr) <= 0) {
    perror("Errore nella funzione inet_pton()");
    exit(1);
}

//Connessione con il server
if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Errore nella funzione connect()");
    exit(1);
}

//Lettura del numero di byte inviati dal Centro Vaccinale utilizzando la funzione FullRead
if (full_read(socket_fd, &dim_benvenuto, sizeof(int)) < 0) {
    perror("Errore nella funzione full_read()");
    exit(1);
}

//Ricezione del messaggio di benvenuto dal Centro Vaccinale
if (full_read(socket_fd, buffer, dim_benvenuto) < 0) {
    perror("Errore nella funzione full_read()");
    exit(1);
}
printf("%s\n", buffer);

//Creazione del pacchetto da inviare al Centro Vaccinale
pacchetto = creazione_pacchetto();

//Invio del pacchetto richiesto al Centro Vaccinale utilizzando la funzione FullWrite
if (full_write(socket_fd, &pacchetto, sizeof(pacchetto)) < 0) {
    perror("Errore nella funzione full_write()");
    exit(1);
}

//Ricezione dell'acknowledgement (ack)
if (full_read(socket_fd, buffer, DIM_ACK) < 0) {
    perror("Errore nella funzione full_read()");
    exit(1);
}
printf("%s\n\n", buffer);

exit(0);
}
