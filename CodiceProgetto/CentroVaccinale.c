#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>  // Libreria che contiene le definizioni dei tipi di dati utilizzati nei sistemi Unix.
#include <sys/socket.h> // Libreria che contiene le definizioni dei socket.
#include <arpa/inet.h>  // Libreria che contiene le definizioni per le operazioni Internet.
#include <unistd.h>    // Libreria che contiene le definizioni di funzioni e costanti standard del sistema Unix.
#include <errno.h>      // Libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <string.h>
#include <netdb.h>      // Libreria che contiene le definizioni per le operazioni del database di rete.
#include <time.h>
#include <signal.h>     // Libreria che consente l'utilizzo delle funzioni per la gestione dei segnali fra processi.

#define DIM_MAX 1024   // Dimensione massima del buffer.
#define DIM_ID 11      // Dimensione del codice di tessera sanitaria (10 byte + 1 byte del carattere terminatore).
#define DIM_ACK 61     // Dimensione della risposta di conferma.


// Si leggono esattamente count byte iterando opportunamente le letture e anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buffer, size_t count) {
    size_t n_left;   // Numero di byte rimanenti da leggere.
    ssize_t n_read;  // Numero di byte letti.
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

//Questa è la struct che useremo per salvare una data.
typedef struct {
    int giorno;
    int mese;
    int anno;
} data;

//Questa è la struct del pacchetto che il centro vaccinale riceve dall'Client.
typedef struct {
    char nome[DIM_MAX];
    char cognome[DIM_MAX];
    char id[DIM_ID];
} Richiesta_Vaccino;


//Questa è la struct del pacchetto che il ServerV riceve dal CentroVaccinale.
typedef struct {
    char id[DIM_ID];
    data data_inizio;
    data scadenza;
} Richiesta_GP;

//Usato per catturare il segnale CTRL-C e stampare un messaggio di arrivederci.
void handler(int signum) {
    switch(signum) {
        case SIGINT:
            printf("\nUscita in corso.\n");
            sleep(2);
            printf("Arrivederci!\n");
            exit(0);
        default:
            printf("Segnale %d ricevuto\n", signum);
    }
}
//Procedura per calcolare la data di inizio del Green Pass.
void crea_data_inizio(data *data_inizio) {
    time_t ticks;
    ticks = time(NULL);

    //Dichiarazione strutture per la conversione della data da stringa ad intero
    struct tm *s_data = localtime(&ticks);
    s_data->tm_mon += 1;           //Sommiamo 1 perchè i mesi vanno da 0 ad 11
    s_data->tm_year += 1900;       //Sommiamo 1900 perchè gli anni partono dal 123 (2023 - 1900)

    printf("Data dell'inizio validità Green Pass: %02d:%02d:%02d\n", s_data->tm_mday, s_data->tm_mon, s_data->tm_year);

    //Assegnamo i valori ai parametri di ritorno
    data_inizio->giorno = s_data->tm_mday ;
    data_inizio->mese = s_data->tm_mon;
    data_inizio->anno = s_data->tm_year;
}
// Procedura per calcolare la data di scadenza del Green Pass
void crea_data_scadenza(data *data_scadenza) {
    time_t ticks;                   // Variabile per la gestione della data
    ticks = time(NULL);             // Estrapoliamo l ora esatta della macchina e la assegniamo alla variabile "ticks"

    // Dichiarazione struttura "tm" per la conversione della data da stringa ad intero
    struct tm *datatmp = localtime(&ticks);

    // Aggiungiamo 4 mesi alla data corrente per ottenere la data di scadenza del green pass, che ha una validità di 6 mesi
    datatmp->tm_mon += 4;

    // Aggiungiamo 1900 all'anno della data corrente, perché la struttura "tm" conta gli anni partendo dal 1900
    datatmp->tm_year += 1900;

    // Effettuiamo il controllo nel caso in cui il vaccino sia stato fatto nel mese di ottobre, novembre o dicembre, comportando un aumento dell'anno
    if (datatmp->tm_mon > 12) {
        // Se il mese supera il dicembre, incrementiamo l'anno e sottraiamo 12 per ottenere il mese corretto
        datatmp->tm_mon -= 12;
        datatmp->tm_year++;
    }

    // Stampiamo la data di scadenza calcolata
    printf("Data scadenza Green Pass: %02d:%02d:%02d\n", datatmp->tm_mday, datatmp->tm_mon, datatmp->tm_year);

    // Assegniamo i valori ai parametri di ritorno
    data_scadenza->giorno = datatmp->tm_mday;
    data_scadenza->mese = datatmp->tm_mon;
    data_scadenza->anno = datatmp->tm_year;
}
/*
 Questa funzione invia al ServerV un Green Pass
  specificando la data di inizio e fine validità e l'ID.
 */
void invio_GP(Richiesta_GP gp) {
    // Dichiarazione e inizializzazione delle variabili
    int socket_fd; // Descrittore del socket
    char bit_start = '1'; // Bit di inizio della comunicazione
    struct sockaddr_in server_addr = {0}; // Inizializziamo a zero la struttura

    // Creazione del descrittore del socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("Errore durante la creazione del socket");
        exit(1);
    }

    // Configurazione della struttura sockaddr_in
    server_addr.sin_family = AF_INET; // Utilizzo della famiglia di protocolli internet IPv4
    server_addr.sin_port = htons(1025); // Porta utilizzata

    // Conversione dell'indirizzo IP da stringa a formato binario
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) == -1) {
        perror("Errore durante la conversione dell'indirizzo IP");
        exit(1);
    }

    // Connessione al serverV
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Errore durante la connessione al server vaccinale");
        exit(1);
    }

    // Invio del bit di inizio della comunicazion
    if (full_write(socket_fd, &bit_start, sizeof(char)) == -1) {
        perror("Errore durante l'invio del bit di inizio");
        exit(1);
    }

    // Invio del Green Pass al ServerV
    if (full_write(socket_fd, &gp, sizeof(gp)) == -1) {
        perror("Errore durante l'invio del Green Pass");
        exit(1);
    }

    // Chiusura del socket
    close(socket_fd);
}

    /*
  La funzione risposta_Client gestisce la richiesta di un Client che si collega al centro vaccinale.
  Vengono inviati al client un messaggio di benvenuto e una richiesta per i dati del green pass.
  Successivamente, viene notificato l'Client dell'avvenuta ricezione dei dati, e viene generato un nuovo
  green pass da inviare al ServerV tramite la funzione invio_GP.
 */
void risposta_Client(int connect_fd) {
// Buffer per l'input dell'Client e dimensione dei pacchetti
char buffer[DIM_MAX];
int indice, dim_benvenuto, dim_pacchetto;
// Elenco dei centri vaccinali disponibili
char *nomi_centri[] = {"Milano", "Roma", "Torino", "Firenze", "Napoli", "Bari", "Palermo"};

// Strutture dati per la richiesta del vaccino e del Green Pass
Richiesta_Vaccino pacchetto;
Richiesta_GP gp;

// Selezioniamo casualmente uno dei centri disponibili
srand(time(NULL));
indice = rand() % 7;

// Costruiamo un messaggio di benvenuto personalizzato per l'Client
snprintf(buffer, DIM_MAX, "Benvenuto nel centro vaccinale di %s\nInserisci il tuo nome, cognome e numero di tessera sanitaria per la registrazione\n", nomi_centri[indice]);
dim_benvenuto = sizeof(buffer);

// Inviamo la dimensione del messaggio di benvenuto
if(full_write(connect_fd, &dim_benvenuto, sizeof(dim_benvenuto)) < 0) {
    perror("full_write() error");
    exit(1);
}

// Inviamo il messaggio di benvenuto
if(full_write(connect_fd, buffer, dim_benvenuto) < 0) {
    perror("full_write() error");
    exit(1);
}

// Riceviamo le informazioni per il Green Pass dall'Client
if(full_read(connect_fd, &pacchetto, sizeof(pacchetto)) < 0) {
    perror("full_read() error");
    exit(1);
}

// Stampa a video le informazioni ricevute
printf("\nInformazioni ricevute\n");
printf("Nome: %s\n", pacchetto.nome);
printf("Cognome: %s\n", pacchetto.cognome);
printf("Numero Tessera Sanitaria: %s\n\n", pacchetto.id);

// Notifica all'Client che i dati sono stati ricevuti correttamente
snprintf(buffer, DIM_ACK, "\nDati registrati con successo!");
if(full_write(connect_fd, buffer, DIM_ACK) < 0) {
    perror("full_write() error");
    exit(1);
}

// Copiamo il numero di tessera sanitaria inviato dall'Client nel Green Pass da inviare al server vaccinale
strcpy(gp.id, pacchetto.id);


crea_data_inizio(&gp.data_inizio);
crea_data_scadenza(&gp.scadenza);
close(connect_fd); // Chiudiamo la connessione con l'Client

// Invio del nuovo Green Pass
invio_GP(gp);
}







int main(int argc, char const *argv[]) {
    // Definizione delle variabili per il socket del server e della connessione
int listen_fd, connect_fd;

// Definizione della struttura che contiene i dati della richiesta di vaccinazione
Richiesta_Vaccino pacchetto;

// Definizione della struttura per i dettagli di connessione del server
struct sockaddr_in serv_addr;

// Identificativo del processo figlio
pid_t pid;

// Cattura del segnale CTRL-C per la chiusura del server
signal(SIGINT,handler);

// Creazione del socket TCP/IP
if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Errore nella creazione del socket");
    exit(1);
}

// Configurazione delle impostazioni di connessione del server
serv_addr.sin_family = AF_INET; // famiglia di protocollo IP
serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // accetta connessioni da qualsiasi indirizzo IP
serv_addr.sin_port = htons(1024); // porta di ascolto del server

// Associazione del socket all'indirizzo e alla porta specificati
if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Errore nell'associazione del socket all'indirizzo e alla porta");
    exit(1);
}

// Abilitazione del server alla ricezione di connessioni in ingresso
if (listen(listen_fd, 1024) < 0) {
    perror("Errore nell'abilitazione del server alla ricezione di connessioni in ingresso");
    exit(1);
}

// Ciclo infinito di attesa di connessioni
for (;;) {

    printf("In attesa di nuove richieste di vaccinazione...\n");

    // Accettazione della connessione in ingresso
    if ((connect_fd = accept(listen_fd, (struct sockaddr *)NULL, NULL)) < 0) {
        perror("Errore nell'accettazione della connessione in ingresso");
        exit(1);
    }

    // Creazione del processo figlio per la gestione della richiesta
    if ((pid = fork()) < 0) {
        perror("Errore nella creazione del processo figlio");
        exit(1);
    }

    if (pid == 0) { // codice eseguito dal figlio

        // Chiusura del socket del server nel processo figlio
        close(listen_fd);

        // Gestione della richiesta di vaccinazione
        risposta_Client(connect_fd);

        // Chiusura del socket di connessione nel processo figlio
        close(connect_fd);

        // Terminazione del processo figlio
        exit(0);

    } else { // codice eseguito dal padre

        // Chiusura del socket di connessione nel processo padre
        close(connect_fd);
    }
}

// Terminazione del processo padre
exit(0);
}

