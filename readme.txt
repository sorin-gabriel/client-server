Definitii:

    - ID_LEN 10 = lungimea id-ului unui client
    - TOPIC_LEN 50 = lungimea titlului unui topic
    - CONTENT_LEN 1500 = lungimea maxima a continutului unui pachet primit de
server de la un client udp
    - INTENT_LEN 100 = lungimea maxima a unui mesaj primit de server de la un
client tcp
    - PROTO_BUFF 2048 = lungimea bufferului folosit pentru comenzi de la stdin

    ! EXIT_IF(cond, desc) =
        testeaza "cond" si daca se evalueaza la TRUE, opreste aplicatia si scrie
mesajul "desc" la stderr;
        macrodefinitia este folosita pentru a verifica toate cazurile de eroare
in cazul operatiilor pe socketi

    - ERR(desc) = scrie mesajul "desc" la stderr

    ! proto_pkt =
        definitia pachetului folosit la nivel aplicatie;
        am folosit union pentru a putea trimite un pachet de la clientul udp
catre clientul tcp cu un cost minim de procesare pe server;
        astfel, de fiecare data cand serverul primeste un pachet de la un client
udp, il va interpreta initial drept "udp_pkt", apoi il va interpreta drept
"notify_pkt" pentru a adauga in pachet adresa ip si portul clientului udp si a
il trimite mai departe la clientul tcp;
        clientul tcp desface singur pachetul "notify_pkt" si printeaza mesajul;
        daca tipul mesajului este EXIT, inseamna ca serverul se inchide si
clientii trebuie sa se deconecteze;
        pentru comunicarea in sens invers (de la clientul tcp catre server), se
foloseste "tcp_pkt", pe care clientul il completeaza cu id-ul si intentia pe
care o are (exit, subscribe [topic] [sf], unsubscribe [topic])

    - content_type = apare intr-un mesaj primit de clientul tcp; poate fi unul
dintre INT, SHORT_REAL, FLOAT, STRING, EXIT; la primirea unui pachet cu type
EXIT, clientul se opreste

    - status = unul dintre OFFLINE, ONLINE; defineste starea unui client la un
moment dat

    - notifications_table = lista de pachete pe care le va primi la reconectare
un client (daca este abonat cu optiunea SF la anumite topicuri)

    - client = defineste un client; retine socketul asociat, id-ul, starea si o
lista de notificari

    - subscription = defineste calitatea de abonat la un anumit topic pentru un
anumit client; contine id-ul clientului si optiunea SF

    - topic = contine titlul topicului si lista de subscriptii

Descriere implementare:

    La pornire, serverul deschide 2 socketi, unul pentru conexiuni tcp si unul
pentru conexiuni udp si primeste conexiuni tcp noi doar daca nu exista deja un
client online cu acelasi id. Tabelele clients si topics sunt dinamice (ambele
sunt initializate cu capacity = 1 si capacitatea se dubleaza cand trebuie
adaugat un item nou si nu mai este spatiu in tabel). Cand serverul primeste
comanda "exit" de la stdin trimite tuturor clientilor conectati un mesaj cu
tipul EXIT, care va determina inchiderea clientilor. Daca un client primeste
de la tastatura comanda "exit", va trimite serverului un pachet cu mesajul
"exit" pentru a fi marcat de server drept offline. Un client offline care are
setata optiunea SF pentru cel putin un topic va primi mesajele la reconectare.
Mesajele sunt pastrate de catre server intr-o lista individuala de mesaje.

Arhitectura server:

  ┏━━━━━━━━━━━ server ━━━━━━━━━━━━┓
  ┃                               ┃
clients                         topics
┃  ┃  ┃                         ┃  ┃  ┃
┃  ┃  client1                   ┃  ┃  topic1
┃  ┃  ┃ ┃ ┃ ┃                   ┃  ┃  ┃    ┃
┃  ┃  ┃ ┃ ┃ sockfd              ┃  ┃  ┃    title
┃  ┃  ┃ ┃ id                    ┃  ┃  subs
┃  ┃  ┃ status                  ┃  ┃  ┃  ┃
┃  ┃  ┃                         ┃  ┃  ┃  sub1
┃  ┃  notifications             ┃  ┃  ┃  ┃  ┃
┃  ┃  ┃ ┃ ┃ ┃                   ┃  ┃  ┃  ┃  id
┃  ┃  ┃ ┃ ┃ pachet1             ┃  ┃  ┃  sf
┃  ┃  ┃ ┃ pachet2               ┃  ┃  ┃
┃  ┃  ┃ pachet3                 ┃  ┃  ...
┃  ┃  ...                       ┃  ┃
┃  ┃                            ┃  ┗━ topic2
┃  ┗━ client2                   ┃     ┃    ┃
┃     ┃ ┃ ┃ ┃                   ...   ┃    title
...   ┃ ┃ ┃ sockfd                    subs
      ┃ ┃ id                          ┃  ┃
      ┃ status                        ┃  sub1
      ┃                               ┃  ┃  ┃
      notifications                   ┃  ┃  id
      ┃ ┃ ┃ ┃                         ┃  sf
      ┃ ┃ ┃ pachet1                   ┃
      ┃ ┃ pachet2                     ...
      ┃ pachet3
      ...

Local results (testat pe IOCLA_VM):

RESULTS
-------
compile...........................passed
server_start......................passed
c1_start..........................passed
data_unsubscribed.................passed
c1_subscribe_all..................passed
data_subscribed...................passed
c1_stop...........................passed
c1_restart........................passed
data_no_clients...................passed
same_id...........................passed
c2_start..........................passed
c2_subscribe......................passed
c2_subscribe_sf...................passed
data_no_sf........................passed
data_sf...........................passed
c2_stop...........................passed
data_no_sf_2......................passed
data_sf_2.........................passed
c2_restart_sf.....................passed
quick_flow........................passed
server_stop.......................passed

real	0m27.251s
user	0m1.223s
sys	0m0.143s
