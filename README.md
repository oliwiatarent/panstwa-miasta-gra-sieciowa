<h1>Gra Państwa-Miasta</h1>

Gracz łączy się do serwera i wysyła swój nick (jeśli nick jest już zajęty,
serwer prosi o podanie innego nicku).

Gracz po wybraniu nicku trafia do lobby, w którym widzi bieżącą listę pokoi,
liczbę graczy która jest w każdym pokoju i stan rozgrywki.

Z lobby gracz może wejść do istniejącego pokoju lub założyć nowy, stając się jego liderem.
Przy zakładaniu pokoju gracz określa nazwę pokoju, liczbę rund oraz graczy. Gracz może
w każdej chwili wrócić z pokoju do lobby.

Lider ma możliwość wyrzucenia gracza z pokoju.

Jeśli w pokoju trwa gra, gracz oczekuje na zakończenie rundy. Jeśli nie, gracz
widzi listę graczy którzy są w pokoju i czeka na rozpoczęcie gry.

Serwer domyślnie rozpoczyna grę po 30 sekundach od dołączenia 3 graczy lub czeka
na sygnał od lidera.

Serwer rozpoczyna rundę, wybiera literkę alfabetu, kategorie i informuje graczy.

Gracz może wpisywać pojedyncze terminy z odgadywanych kategorii. Jeśli gracz
uzna, że jest gotowy, może nacisnąć przycisk kończący rundę po 10 sekundach.
Jeśli żaden gracz nie zgłosi gotowości, runda kończy się po 3 minutach.

Kiedy czas dobiegnie końca, serwer prezentuje odpowiedzi wszystkich graczy
i przydziela odpowiednią punktację premiując czas i unikalność odpowiedzi.
Po 60 sekundach serwer rozpoczyna nową rundę.

Kiedy po określonej liczbie rund gra dobiegnie końca, serwer podsumowuje końcową
punktację i przedstawia ją wszystkim graczom. Następnie serwer przenosi graczy
do lobby.

Gra trwa do momentu aż w pokoju jest przynajmniej trzech graczy lub zostanie
rozegrana określona liczba rund. Gracze na bieżąco widzą ranking graczy z punktami,
włączając w to też graczy, którzy rozłączyli się z gry.

Administrator może przejrzeć wszystkie aktywne gry. W ramach pojedynczego pokoju
przed rozpoczęciem gry może modyfikować jego ustawienia, zmieniać nazwę lub
całkowicie go usunąć. Ma również możliwość wyrzucania graczy z pokojów.

Po rozpoczęciu gry administrator ma możliwość usunięcia pokoju, zmiany liczby rund oraz wyrzucania graczy z pokojów.
