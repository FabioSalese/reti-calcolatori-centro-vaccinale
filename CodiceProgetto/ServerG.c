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

#define DIM_MAX 1024  // Dimensione massima del buffer.
#define DIM_BENVENUTO 108
#define DIM_ID 11    // Dimensione del codice di tessera sanitaria (10 byte + 1 byte del carattere terminatore).
#define DIM_ACK 64 // Dimensione della risposta di conferma.
#define DIM_ACK2 39

//Struct che consente di memorizzare una data rappresentata da tre campi: giorno, mese e anno.
typedef struct {
    int giorno;
    int mese;
    int anno;
} data;

//Struct del pacchetto trasmesso dal centro vaccinale al server vaccinale include il numero di tessera sanitaria del Client e le date di inizio e fine della validità del GP.
typedef struct {
    char ID[DIM_ID];
    char report;
    data data_inizio;
    data data_scadenza;
} Richiesta_GP;

//Struct del pacchetto che viene inviato dal ClienT include il numero di tessera sanitaria del green pass e il referto che attesta la sua validità.
typedef struct  {
    char ID[DIM_ID];
    char report;
} REPORT;


// Attraverso un'opportuna iterazione delle letture, la funzione legge esattamente "count" byte, anche in caso di interruzione da parte di una System Call.
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
    buffer = 0;    // Imposta il buffer a zero
    return n_left;   // Restituisce il numero di byte rimanenti da leggere.
}


// Attraverso un'opportuna iterazione delle scritture, la funzione scrive esattamente "count" byte, anche in caso di interruzione da parte di una System Call.
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

//Questa funzione è utilizzata per estrarre la data corrente dal sistema e sarà impiegata per le operazioni di verifica del Green Pass.
void crea_data_inizio(data *data_inizio) {
    time_t ticks;
    ticks = time(NULL);

    //Vengono dichiarate le strutture necessarie per convertire una data da stringa ad intero.
    struct tm *s_date = localtime(&ticks);
    s_date->tm_mon += 1;           //Aggiungiamo 1 poiché i mesi sono indicizzati da 0 a 11
    s_date->tm_year += 1900;       //Aggiungiamo 1900 poiché gli anni sono rappresentati come numeri a due cifre, partendo dal 1900 (ad esempio, per l'anno 2023 si ha 23).

    //Vengono attribuiti i valori ai parametri di ritorno.
    data_inizio->giorno = s_date->tm_mday ;
    data_inizio->mese = s_date->tm_mon;
    data_inizio->anno = s_date->tm_year;
}

 /* La funzione ha il compito di scansionare il certificato verde, ricevendo dal ClientS un numero di tessera sanitaria. Successivamente, richiede al ServerV il report, esegue le procedure di verifica necessarie e infine comunica l'esito dell'operazione al ClientS ."*/
char verifica_ID(char ID[]) {
    int socket_fd, dim_benvenuto, dim_pacchetto;
    struct sockaddr_in server_addr;
    char buffer[DIM_MAX], report, bit_start;
    Richiesta_GP gp;
    data data_corrente;

    //Per indicare che la comunicazione avviene con il ServerG, impostiamo il valore di 'bit_start' a 0 prima di inviare i dati al ServerV."
    bit_start = '0';

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1025);

     //Convertiamo un indirizzo IP, che viene fornito in input come una stringa in formato dotted, in un indirizzo di rete utilizzando l'ordine dei byte in network order.
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton error");
        exit(1);
    }

    // Connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }

    //Trasmettiamo un bit con valore 0 a ServerV per indicare che la comunicazione deve avvenire con ServerG.
    if (full_write(socket_fd, &bit_start, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    bit_start = '1';

    //Trasmettiamo un bit con valore 1 a ServerV per richiedere la verifica del green pass.
    if (full_write(socket_fd, &bit_start, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Trasmettiamo il numero di tessera sanitaria ricevuto dal ClientS al SeverV
    if (full_write(socket_fd, ID, DIM_ID)) {
        perror("full_write() error");
        exit(1);
    }

    //Riceviamo il report dal ServerV
    if (full_read(socket_fd, &report, sizeof(char)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    if (report == '1') {
        //Se il valore ricevuto dal ServerV è 0, la verifica non è valida. Se invece il valore ricevuto è 1, allora la verifica è considerata valida.
        if (full_read(socket_fd, &gp, sizeof(Richiesta_GP)) < 0) {
            perror("full_read() error");
            exit(1);
        }

        close(socket_fd);

        //Funzione per la data
        crea_data_inizio(&data_corrente);

        /*
         Se l'anno corrente è maggiore dell'anno di scadenza del green pass, il green pass non è considerato valido e il valore di report viene impostato a 0.
         Allo stesso modo, se l'anno di scadenza è lo stesso, ma il mese corrente è successivo al mese di scadenza del green pass, o se entrambi gli anni e i mesi sono validi ma il giorno corrente è successivo al giorno di scadenza del green pass, il green pass non è considerato valido e il valore di report viene impostato a 0.
         */
        if (data_corrente.anno > gp.data_scadenza.anno) report = '0';
        if (report == '1' && data_corrente.mese > gp.data_scadenza.mese) report = '0';
        if (report == '1' && data_corrente.giorno > gp.data_scadenza.giorno) report = '0';
        if (report == '1' && gp.report == '0') report = '0'; //Il Green Pass non è considerato valido se, nonostante la sua validità temporale, il report del tampone risulta negativo.
    }

    return report;
}

//Funzione per la gestione dell'interazione con il Client.
void ricezione_ID(int connect_fd) {
    // Dichiarazione delle variabili locali
    char report, buffer[DIM_MAX], ID[DIM_ID];
    int dim_benvenuto, dim_pacchetto;

    // Alla connessione con il ServerG, viene inviato un messaggio di benvenuto al ClientS
    snprintf(buffer, DIM_BENVENUTO, "Benvenuto nel server di verifica\nInserisci il numero della tessera sanitaria per verificarne la validità.");
    buffer[DIM_BENVENUTO - 1] = 0;  // Imposta l'ultimo carattere del buffer a 0
    if(full_write(connect_fd, buffer, DIM_BENVENUTO) < 0) {  // Invia il messaggio di benvenuto all'ClientS
        perror("full_write() error");
        exit(1);
    }

    // Riceve il numero di tessera sanitaria dal ClientS
    if(full_read(connect_fd, ID, DIM_ID) < 0) {  // Riceve i dati dal client
        perror("full_read error");
        exit(1);
    }

    // Invia una notifica di conferma al client
    snprintf(buffer, DIM_ACK, "numero di tessera sanitaria  ricevuto");
    buffer[DIM_ACK - 1] = 0;
    if(full_write(connect_fd, buffer, DIM_ACK) < 0) {  // Invia una conferma di ricezione al client
        perror("full_write() error");
        exit(1);
    }

    // Invoca la funzione verifica_ID per verificare la validità del numero di tessera sanitaria
    report = verifica_ID(ID);

    // Invia il report di validità del green pass al ClientS
    if (report == '1') {  // Se il report è "1" (verde)
        strcpy(buffer, "Green Pass valido!");
        if(full_write(connect_fd, buffer, DIM_ACK2) < 0) {  // Invia una risposta al client
            perror("full_write() error");
            exit(1);
        }
    } else if (report == '0') {  // Se il report è "0" (rosso)
        strcpy(buffer, "Il Green Pass non è valido!");
        if(full_write(connect_fd, buffer, DIM_ACK2) < 0) {  // Invia una risposta al client
            perror("full_write() error");
            exit(1);
        }
    } else {  // Se il report non è né "1" né "0"
        strcpy(buffer, "Numero tessera sanitaria non esiste");
        if(full_write(connect_fd, buffer, DIM_ACK2) < 0) {  // Invia una risposta al client
            perror("full_write() error");
            exit(1);
        }
    }

    // Chiude la connessione con il client
    close(connect_fd);
}

// Funzione per inviare il report del green pass al server di verifica
char invio_report(REPORT pacchetto) {
    int socket_fd;
    struct sockaddr_in server_addr;
    char bit_start, buffer[DIM_MAX], report;

    // Valore iniziale del bit di inizio
    bit_start = '0';

    // Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    // Valorizzazione struttura indirizzo del server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1025);  // Porta del server di verifica

    // Conversione dell'indirizzo IP dal formato dotted decimal a stringa binaria
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton() error");
        exit(1);
    }

    // Connessione con il ServerG
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }

    // Invia un bit di valore 0 al ServerG per informarlo che deve comunicare con il ServerV
    if (full_write(socket_fd, &bit_start, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    // Invia un bit di valore 0 al ServerG per informarlo che deve modificare il report del green pass
    if (full_write(socket_fd, &bit_start, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    // Invia il pacchetto appena ricevuto dal ClientS al ServerG
    if (full_write(socket_fd, &pacchetto, sizeof(REPORT)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    // Riceve il report dal ServerG
    if (full_read(socket_fd, &report, sizeof(report)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    close(socket_fd); // Chiude la connessione col ServerG

    return report; // Restituisce il report del green pass
}

void ricezione_report(int connect_fd) {
    REPORT pacchetto; // Struttura del pacchetto REPORT
    char report, buffer[DIM_MAX]; // Variabili per la gestione del report e del buffer di comunicazione

    // Legge i dati contenuti nel pacchetto REPORT inviato dal ClientT
    if (full_read(connect_fd, &pacchetto, sizeof(REPORT)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    // Invia il pacchetto al ServerV al fine di ottenere il report
    report = invio_report(pacchetto);

    // In base al report ottenuto, invia una risposta al client
    if (report == '1') {
        // Se il report è '1', significa che il numero di tessera sanitaria non esiste
        strcpy(buffer, "Tessera sanitaria non trovata");

        // Invia il messaggio di errore al client
        if(full_write(connect_fd, buffer, DIM_ACK2) < 0) {
            perror("full_write() error");
            exit(1);
        }
    } else {
        // Se il report è diverso da '1', significa che l'operazione è avvenuta con successo
        strcpy(buffer, "Operazione conclusa con successo!");

        // Invia il messaggio di conferma al client
        if(full_write(connect_fd, buffer, DIM_ACK2) < 0) {
            perror("full_write() error");
            exit(1);
        }
    }
}

int main() {
    int listen_fd, connect_fd;
    struct sockaddr_in serv_addr;
    pid_t pid;
    char bit_start;

    signal(SIGINT,handler); //Rileva il segnale CTRL-C.
    //Creazione descrizione del socket
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione strutture
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(1026);

    //Assegna la porta al server.
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind() error");
        exit(1);
    }

    //Mette il socket in modalità ascolto, in attesa di nuove connessioni.
    if (listen(listen_fd, 1024) < 0) {
        perror("listen() error");
        exit(1);
    }

    for (;;) {
    printf("In attesa del Green Pass per la scansione\n");


        //Accettiamo una nuova connessione
        if ((connect_fd = accept(listen_fd, (struct sockaddr *)NULL, NULL)) < 0) {
            perror("accept() error");
            exit(1);
        }

        //Creazione del figlio;
        if ((pid = fork()) < 0) {
            perror("fork() error");
            exit(1);
        }

        if (pid == 0) {
            close(listen_fd);

            /*
             Il ServerG riceve un bit come primo messaggio, il cui valore può essere 0 o 1, a seconda della connessione richiesta.
             Se il bit ricevuto è 1, la connessione con il ClientT viene gestita dal figlio. Se invece il bit è 0, la connessione con il ClientS viene gestita dal figlio.
            */
            if (full_read(connect_fd, &bit_start, sizeof(char)) < 0) {
                perror("full_read() error");
                exit(1);
            }
            if (bit_start == '1') ricezione_report(connect_fd);   //Riceviamo informazioni dal ClienT
            else if (bit_start == '0') ricezione_ID(connect_fd);  //Riceviamo informazioni dal ClienS
            else printf("Client non riconosciuto\n");

            close(connect_fd);
            exit(0);
        } else close(connect_fd);
    }
    exit(0);
}
