#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h> // Libreria che contiene le definizioni dei socket.
#include <arpa/inet.h> // Libreria che contiene le definizioni per le operazioni Internet.
#include <unistd.h>  // Libreria che contiene le definizioni di funzioni e costanti standard del sistema Unix.
#include <errno.h>      // Libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <string.h>
#include <fcntl.h>      // Libreria che contiene opzioni di controllo per i file
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <signal.h>  // Libreria che consente l'utilizzo delle funzioni per la gestione dei segnali fra processi.

#define DIM_MAX 2048     // Dimensione massima del buffer.
#define DIM_ID 11       // Dimensione del codice di tessera sanitaria (10 byte + 1 byte del carattere terminatore).

// Definizione di una struttura dati chiamata "DATA". La struttura contiene tre campi: "giorno", "mese" e "anno".  Questa struttura viene utilizzata per rappresentare una data.
typedef struct {
    int giorno;
    int mese;
    int anno;
} DATA;

// Definizione di una struttura dati chiamata "RICHIESTA_GP".
// La struttura contiene informazioni per inviare una richiesta di green pass dal centro vaccinale al server vaccinale. Le informazioni includono il numero di tessera sanitaria dell'Client e la data di inizio e fine validità del green pass.
typedef struct {
    char ID[DIM_ID]; // Array di caratteri che rappresenta il numero di tessera sanitaria dell'Client.
    char report; // Variabile che rappresenta la validità del green pass: 0 = non valido, 1 = valido.
    DATA data_inizio; // Variabile che rappresenta la data di inizio validità del green pass.
    DATA data_scadenza; // Variabile che rappresenta la data di fine validità del green pass.
} RICHIESTA_GP;


// Definizione di una struttura dati chiamata "REPORT".
// La struttura contiene due campi: "ID" e "report". Questa struttura viene utilizzata per rappresentare il pacchetto dell'ClientT contenente il numero di tessera sanitaria di un green pass ed il suo referto di validità.
typedef struct  {
    char ID[DIM_ID]; // Il numero di tessera sanitaria del green pass
    char report; // Il referto di validità del green pass. Può essere 0 per "non valido" o 1 per "valido".
} REPORT;


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


//Funzione che tratta la comunicazione con il CentroVaccinale e salva i dati ricevuti da questo in un filesystem.
void comunicazione_CentroV(int connect_fd) {
    int fd;
    RICHIESTA_GP gp;

   // Ricezione del green pass dal CentroVaccinale.
if (full_read(connect_fd, &gp, sizeof(RICHIESTA_GP)) < 0) {
    perror("Errore durante la lettura del green pass");
    exit(1);
}

// Il green pass è valido di default quando viene generato.
gp.report = '1';

/*
 Per ogni tessera sanitaria viene creato un file contenente i dati ricevuti.
 L'apertura del file viene effettuata in modalità di scrittura (O_WRONLY), creazione se il file non esiste (O_CREAT) e troncamento del file a zero se esiste già (O_TRUNC).
 Il valore 0777 rappresenta i permessi del file appena creato.
 In particolare, 7 rappresenta i permessi di lettura, scrittura ed esecuzione per il proprietario del file, 7 rappresenta i permessi di lettura, scrittura ed esecuzione per il gruppo a cui appartiene il file, e infine 7 rappresenta i permessi di lettura, scrittura ed esecuzione per tutti gli altri utenti.
 */
fd = open(gp.ID, O_WRONLY| O_CREAT | O_TRUNC, 0777);
if (fd < 0) {
    perror("Errore durante l'apertura del file");
    exit(1);
}
fd = open(gp.ID, O_WRONLY| O_CREAT | O_TRUNC, 0777);
if (fd < 0) {
    perror("Errore durante l'apertura del file");
    exit(1);
}

// Scrittura dei campi del green pass all'interno di un file binario, il cui nome corrisponde al numero di tessera sanitaria del green pass.
if (write(fd, &gp, sizeof(RICHIESTA_GP)) < 0) {
    perror("Errore durante la scrittura del file");
    exit(1);
}

// Chiusura del file.
close(fd);
}
//Una funzione che, su richiesta dell'ClientT, apporta modifiche al GP.
void modifica_report(int connect_fd) {
    REPORT package; // struttura che contiene il referto del tampone e il numero di tessera sanitaria
    RICHIESTA_GP gp; // struttura che contiene le informazioni del green pass
    int fd;
    char report; // carattere che rappresenta il report dell'operazione

    // riceve un pacchetto dal ServerG, proveniente dall'ClientT, contenente il numero di tessera sanitaria e il referto del tampone
    if (full_read(connect_fd, &package, sizeof(REPORT)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    // apre il file contenente il green pass corrispondente al numero di tessera sanitaria ricevuto dall'ClientT
    fd = open(package.ID, O_RDWR , 0777);

    /*
        Se il numero di tessera sanitaria inviato dall'ClientT non esiste, la variabile globale errno registra l'errore.
        In tal caso, viene inviato un report uguale a 1 al ServerG, che a sua volta aggiornerà l'ClientT dell'inesistenza del codice.
        In caso contrario, viene inviato un report uguale a 0 per indicare che l'operazione è avvenuta correttamente.
    */
    if (errno == 2) {
        printf("Il numero di tessera sanitaria inserito non è presente nei nostri archivi, per favore riprova...\n");
        report = '1';
    } else {
        // acquisisce un lock esclusivo sul file per evitare accessi simultanei
        if(flock(fd, LOCK_EX | LOCK_NB) < 0) {
            perror("flock() error");
            exit(1);
        }

        // legge il Green Pass dal file precedentemente aperto
        if (read(fd, &gp, sizeof(RICHIESTA_GP)) < 0) {
            perror("read() error");
            exit(1);
        }

        // assegna il referto ricevuto dall'ClientT al Green Pass
        gp.report = package.report;

        // posiziona il puntatore di lettura/scrittura del file all'inizio del file per sovrascrivere tutti i dati precedenti del Green Pass
        lseek(fd, 0, SEEK_SET);

        // scrive i dati modificati nel file binario di Green Pass
        if (write(fd, &gp, sizeof(RICHIESTA_GP)) < 0) {
            perror("write() error");
            exit(1);
        }

        // rilascia il lock acquisito in precedenza
        if(flock(fd, LOCK_UN) < 0) {
            perror("flock() error");
            exit(1);
        }
        report = '0';
    }

    // invia il report dell'operazione al ServerG
    if (full_write(connect_fd, &report, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }
}
//Funzione che invia un GP richiesto dal ServerG
void invia_gp(int connect_fd) {
    char report, ID[DIM_ID];
    int file_descriptor; // File descriptor
    RICHIESTA_GP gp;

    // Riceve il numero di tessera sanitaria dal ServerG.
if (full_read(connect_fd, ID, DIM_ID) < 0) {
    perror("Errore in full_read()");
    exit(1);
}

// Apre il file corrispondente al numero di tessera sanitaria ricevuto dal ServerG.
file_descriptor = open(ID, O_RDONLY, 0777);

// Se il numero di tessera sanitaria non è presente, il valore di errno viene impostato a 2 e viene inviato un report con valore 1 al ServerG. In caso contrario, il valore di errno rimane invariato e viene inviato un report con valore 0 per indicare che l'operazione è avvenuta con successo.
if (errno == 2) {
    printf("Il numero di tessera sanitaria inserito non è presente nei nostri archivi, per favore riprova...\n");
    report = '2';
    // Invia il report al ServerG.
    if (full_write(connect_fd, &report, sizeof(char)) < 0) {
        perror("Errore in full_write()");
        exit(1);
    }
} else {
    // Accede al file in lettura in maniera esclusiva per evitare accessi simultanei.
    if (flock(file_descriptor, LOCK_EX) < 0) {
        perror("Errore in flock()");
        exit(1);
    }
    // Legge il Green Pass dal file.
    if (read(file_descriptor, &gp, sizeof(RICHIESTA_GP)) < 0) {
        perror("Errore in read()");
        exit(1);
    }
    // Rilascia il lock.
    if(flock(file_descriptor, LOCK_UN) < 0) {
        perror("Errore in flock()");
        exit(1);
    }

    close(file_descriptor);
    report = '1';
    // Invia un segnale al ServerG contenente il report.
    if (full_write(connect_fd, &report, sizeof(char)) < 0) {
        perror("Errore in full_write()");
        exit(1);
    }

    // Trasmette al ServerG il Green Pass richiesto, che verificherà la sua validità.
    if(full_write(connect_fd, &gp, sizeof(RICHIESTA_GP)) < 0) {
        perror("Errore in full_write()");
        exit(1);
    }
}
}


/*
 Funzione che gestisce la comunicazione con il ServerG, recuperando il Green Pass dal file system corrispondente al numero di tessera sanitaria ricevuto e lo invia al ServerG.
*/
void comunicazione_ServerV(int connect_fd) {
    char bit_start;

    /* Riceviamo dal ServerG un bit, che può assumere valore 0 o 1, in base alla funzione richiesta.
     * Se il bit ricevuto è 0, gestiamo la funzione per modificare il report di un Green Pass.
     * Se il bit ricevuto è 1, gestiamo la funzione per inviare un Green Pass al ServerG.
     */
    if (full_read(connect_fd, &bit_start, sizeof(char)) < 0) {
        perror("Errore full_read");
        exit(1);
    }
    if (bit_start == '0') {
        modifica_report(connect_fd);
    } else if (bit_start == '1') {
        invia_gp(connect_fd);
    } else {
        printf("Dato non valido.\n\n");
    }
}




//Usato per catturare il segnale CTRL-C e stampare un messaggio di arrivederci.
void handler(int signum) {
    switch(signum) {
        case SIGINT:
            printf("\nAttendere. Uscita in corso.\n");
            sleep(2);
            printf("Arrivederci!\n");
            exit(0);
        default:
            printf("Segnale %d ricevuto\n", signum);
    }
}
int main() {
    int listen_fd, connect_fd, dim_pacchetto;
    struct sockaddr_in serv_addr;
    pid_t pid;
    char bit_start;
    signal(SIGINT,handler); // Registra la funzione "handler" per catturare il segnale CTRL-C

    // Creazione del socket di ascolto
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    // Configurazione dell'indirizzo del server
    serv_addr.sin_family = AF_INET; // Utilizziamo gli indirizzi IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Ascoltiamo connessioni da qualsiasi indirizzo associato al server
    serv_addr.sin_port = htons(1025); // Numero di porta del socket di ascolto

    // Assegnazione della porta al socket
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind() error");
        exit(1);
    }

    // Socket in ascolto in attesa di nuove connessioni
    if (listen(listen_fd, 1024) < 0) {
        perror("listen() error");
        exit(1);
    }

    // Inizio del ciclo infinito in attesa di nuove connessioni
    for (;;) {
        printf("In attesa di nuove connessioni...\n");

        // Accettazione di una nuova connessione
        if ((connect_fd = accept(listen_fd, (struct sockaddr *)NULL, NULL)) < 0) {
            perror("accept() error");
            exit(1);
        }

        // Creazione del processo figlio per gestire la connessione
        if ((pid = fork()) < 0) {
            perror("fork() error");
            exit(1);
        }

        // Codice eseguito dal figlio
        if (pid == 0) {
            close(listen_fd);

            /*
            Il Server riceve un primo messaggio sotto forma di bit che può essere 0 o 1, poiché sono presenti due connessioni distinte.
            Se il bit ricevuto è 1, il figlio si occuperà della connessione con il Centro Vaccinale,
            mentre se il bit ricevuto è 0, il figlio si occuperà della connessione con il Server Verifica.
            */
            if (full_read(connect_fd, &bit_start, sizeof(char)) < 0) {
                perror("full_read() error");
                exit(1);
            }
            if (bit_start == '1') comunicazione_CentroV(connect_fd);
            else if (bit_start == '0') comunicazione_ServerV(connect_fd);
            else printf("Client non riconosciuto, riprovare\n");

            close(connect_fd);
            exit(0);
        } else {
            // Codice eseguito dal padre
            close(connect_fd);
        }
    }
    exit(0);
}



