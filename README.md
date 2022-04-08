### Scenariusz 7 - serwery korzystające z poll
## Współbieżny jednowątkowy serwer TCP

Wielu klientów jednocześnie może być obsługiwanych przez pojedynczy wątek, który utrzymuje wiele otwartych połączeń i obsługuje klientów w miarę napływu danych.

Jeśli serwer ma obsługiwać wiele połączeń, z których każde korzysta z innego gniazda, to musi mieć możliwość oczekiwania na przychodzące dane na wielu gniazdach jednocześnie. Można to zrealizować, używając wywołań systemowych `select()` lub `poll()` – wybierzemy `poll()` jako wygodniejsze w obsłudze. Funkcja `poll()` operuje na deskryptorach dowolnego typu, niezależnie czy odnoszą się do gniazda, łącza czy standardowego wejścia lub wyjścia.

## Funkcja poll()

```c
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```
W wywołaniu funkcji podaje się wskaźnik do tablicy struktur `pollfd` (parametr `fds`), liczbę struktur w tablicy `fds`m8u (parametr `nfds`) oraz czas oczekiwania, po którym nastąpi powrót z funkcji, jeśli żaden z deskryptorów nie zmieni stanu. Parametr timeout jest wyrażony w milisekundach. Podanie ujemnej liczby milisekund oznacza nieskończone oczekiwanie, a podanie wartości zero spowoduje natychmiastowy powrót z funkcji.

Struktura `pollfd` opisuje jeden obserwowany deskryptor.
```c
struct pollfd {
    int fd;
    short events;
    short revents;
};
```

### Pola:

- `fd` – numer obserwowanego deskryptora, jeśli jest ujemny, to nie jest obserwowany
- `events` – flagi obserwowane zdarzenia dla `fs`
- `revents` – flagi zdarzeń, które zaszły dla fd w czasie wywołania `poll()`

W polu events można ustawić następujące flagi:

- `POLLIN` – obserwowanie nadejścia danych do odczytu
- `POLLOUT` – obserwowanie możliwości zapisania danych
- `POLLPRI` – obserwowanie nadejścia danych wyjątkowych (out-of-band)

Po wywołaniu funkcji `poll()` w polu revents ustawione są flagi oznaczające zdarzenia, które zaszły. Mogą to być flagi `POLLIN`, `POLLOUT` i `POLLPRI` - oznaczają wtedy zajście odpowiadającego im zdarzenia. Oprócz tego mogą zostać ustawione również inne flagi:

- `POLLERR` – wystąpił błąd
- `POLLHUP` – rozłączenie
- `POLLNVAL` – niewłaściwy deskryptor

Funkcja `poll()` przekazuje w wyniku liczbę deskryptorów, dla których zaszło jakieś zdarzenie (>0), 0 jeśli minął czas oczekiwania, a -1 w przypadku błędu.

Schemat działania serwera jest następujący:

```c
struct pollfd fds[N];

for (i = 0; i < N; ++i) {
   fds[i].fd = -1;
   fds[i].events = POLLIN;
   fds[i].revents = 0;
}

fds[0].fd = socket();    
bind();
listen();

do {

  poll(fds, N, -1);

  if (fds[0].revents & POLLIN) {
    fds[0].revents = 0;
    s = accept(fds[0].fd, ...)
    // umieść deskryptor s w tablicy fds 
  }

  for (k = 1; k < N; ++k)
     if (fds[k].revents & POLLIN | POLLERR ) {        
         fds[k].revents = 0;     
         // obsłuż klienta na gnieździe fds[k].fd 
         // jeżeli read przekaże wartość <=0, usuń pozycję k z fds       
}
```

Jeżeli zdarzenie zaszło na deskryptorze głównym `fds[0].fd`, to znaczy, że nowy klient chce się połączyć. W takim przypadku serwer musi zaakceptować nowe połączenie za pomocą funkcji `accept`, a deskryptor nowego gniazda należy dodać do zbioru obserwowanych deskryptorów, czyli umieścić w tablicy deskryptorów `fds`.

Jeżeli zdarzenie zaszło na którymś z pozostałych deskryptorów, to należy odczytać dane, które zostały przesłane przez któregoś z wcześniej połączonych klientów. Należy rozpatrzyć różne przypadki. Jeśli `read` dla danego deskryptora przekaże w wyniku wartość ujemną (błąd) lub 0 (rozłączenie klienta), to deskryptor należy usunąć z tablicy `fds`.

## Ćwiczenia

1. Przeanalizuj kod przykładowego serwera z `pliku poll_server.c`.

2. Uruchom serwer i co najmniej dwóch klientów.

3. Serwer co 5 sekund wypisuje komunikat "Do something else". Zmień argumenty funkcji `poll()` tak, aby oczekiwał na połączenia aż do skutku.

4. Po odebraniu sygnału SIGINT serwer przestaje przyjmować nowe połączenia. Obsługuje klientów połączonych do tej pory, aż do rozłączenia się ostatniego z nich. Sprawdź w jaki sposób zostało to zrealizowane.

## Nieblokujący tryb gniazd
### Działanie gniazd w domyślnym trybie

Zazwyczaj wywołanie funkcji `write()` powoduje skopiowanie danych podanych w argumencie do bufora. Następuje powrót z funkcji i nasz proces może działać dalej, a w tle następuje stopniowe wysyłanie przez sieć danych z bufora.

Co jednak się stanie, gdy dane do funkcji `write()` będą napływały szybciej, niż będzie możliwe ich wysyłanie przez sieć? Wówczas bufor wysyłania zapełni się i przy wywołaniu funkcji `write()` nastąpi zablokowanie procesu, aż do momentu, gdy dane z argumentu będą mogły zostać skopiowane do bufora. W tym czasie nasz proces nie będzie mógł robić nic innego, na przykład obsługiwać pozostałych połączeń. Problem ten występuje szczególnie w sytuacji, gdy głównym zadaniem naszego procesu jest wysyłanie dużych ilości danych, a także gdy odbiorca po drugiej stronie zachowuje się dziwnie, na przykład wysyła do nas różne żądania, ale nie odbiera tego, co mu wysyłamy - wtedy może nawet nastąpić całkowite zawieszenie naszego procesu.

Częściowo pomaga użycie funkcji `poll()` i sprawdzenie, czy dla danego gniazda jest ustawiona flaga POLLOUT (możliwość zapisania danych). Częściowo, dlatego że obecność tej flagi oznacza jedynie, że bufor nie jest całkiem pełny. Nie oznacza, że całe dane podane do funkcji `write()` się w nim zmieszczą, czyli że proces wywołując `write()` nie zostanie zablokowany.

### Ustawianie limitu czasu na zapis

Jednym ze sposobów na radzenie sobie z opisanym wyżej problemem jest ustawienie maksymalnego czasu, jaki może działać funkcja 'write()'. Można to zrobić korzystając z możliwości ustawiania opcji gniazd:

```c
setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout))
```

gdzie `sock` to deskryptor gniazda, natomiast `timeout()` jest zmienną typu `timeval` zawierającą limit czasu, który chcemy ustawić (podobnie, jako trzeci argument podając `SO_RCVTIMEO`, możemy ustawić limit czasu działania funkcji `read()`). Jeśli nie uda się zakończyć zapisu w ustawionym czasie, funkcja `write()` kończy się. Przekazuje w wyniku liczbę zapisanych bajtów (jeśli część udało się zapisać – wtedy przy kolejnym wywołaniu możemy zapisać resztę), albo -1, ustawiając błąd w `errno` na `EAGAIN` lub `EWOULDBLOCK`. Rozwiązanie takie zastosowane zostało w pliku `echo-client-timeout.c`.

### Tryb nieblokujący

Innym podejściem jest przełączenie gniazda w tryb nieblokujący. Robimy to poleceniem:

```c
fcntl(sock, F_SETFL, O_NONBLOCK)
```

gdzie `sock` to deskryptor gniazda. W tym trybie każde wywołanie funkcji `read()` i `write()` natychmiast się kończy. Jeśli nie da się nic odebrać lub wysłać natychmiast, zwracają one -1, ustawiając błąd w `errno` na `EAGAIN` lub `EWOULDBLOCK`. Natomiast funkcja `write()` może zapisać część danych i zwrócić ich rozmiar (wtedy kolejnym wywołaniem `write()` możemy wysłać resztę). Przykład ilustrujący takie podejście znajduje się w pliku `echo-serwer-nonblocking.c`.

W trybie nieblokującym inne jest także zachowanie funkcji `connect()`: nie czeka ona na nawiązanie połączenia, tylko kończy się od razu, a połączenie jest nawiązywane w tle.

### Ćwiczenia

1. Poeksperymentuj z dołączonymi programami. Uruchom je na różnych komputerach i przekieruj do klienta duży plik. Zobacz, po ilu wysłanych bajtach, nieodbieranych przez drugą stronę, `write()` się blokuje.

2. Przekonaj się, że rzeczywiście `write()` na gnieździe nieblokującym lub z ustawionym limitem czasu może zakończyć się po wysłaniu części danych. Ile co najmniej danych do wysłania udaje się umieścić w buforze poleceniem `write()` po tym, gdy `poll()` ustawi flagę `POLLOUT`?

## Ćwiczenie punktowane (1.5 pkt)

W oparciu o program `poll-server` napisz program `poll-server-count`, którego działanie jest rozszerzone o możliwość podania liczby aktualnie połączonych klientów oraz łącznej liczby klientów obsłużonych od początku działania serwera. Pozostała funcjonalność serwera pozostaje bez zmian.

Serwer przyjmuje dwa parametry - numer portu, na którym obsługuje klientów (`client` z aktualnego scenariusza) oraz numer portu kontrolnego (przykładowe wywołanie: `./poll-server-count 10001 2323`).

Kiedy z serwerem łączy się na port kontrolny program nc i poprzez nc zostanie przesłany napis `count`, wtedy serwer odsyła informację w postaci:

```console
Number of active clients: x
Total number of clients: y
```

po czym zamyka połączenie. Napisy inne niż `count` są ignorowane i połączenie jest również zamykane. Połączenia kontrolne nie liczą się jako połączenia z klientami.

Przetestuj swoje rozwiązanie uruchamiając serwer, klientów i program `nc` na różnych komputerach.
