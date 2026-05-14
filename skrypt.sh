#!/bin/bash

# Nazwa pliku wyjściowego
TASKFILE="taskfile"

# Usunięcie starego pliku, jeśli istnieje
> "$TASKFILE"

echo "Generowanie zadań do pliku: $TASKFILE"

# Definicja 3 przykładowych komend testowych
# 1. Prosta komenda (stdout)
# 2. Potok (pipe) sprawdzający liczbę procesów
# 3. Komenda generująca błąd (stderr)
COMMANDS=(
    "date"
    "ps aux | wc -l"
    "ls -l /folder/ktory/nie/istnieje"
)

# Definicja parametrów 'info' dla powyższych komend
# 0 - stdout, 1 - stderr, 2 - oba
INFOS=(0 0 1)

for i in {0..2}
do
    # Obliczanie czasu: obecny czas + (i + 1) minut
    # Format %H:%M (godzina:minuta)
    TARGET_TIME=$(date -d "+$((i + 1)) minute" +"%H:%M")
    
    # Wyciągnięcie godziny i minuty (usuwamy wiodące zera, by uniknąć problemów z systemem ósemkowym w niektórych parserach)
    HOUR=$(echo $TARGET_TIME | cut -d: -f1 | sed 's/^0//')
    MIN=$(echo $TARGET_TIME | cut -d: -f2 | sed 's/^0//')
    
    # Jeśli po usunięciu zera string jest pusty (było "00"), ustawiamy "0"
    [ -z "$HOUR" ] && HOUR="0"
    [ -z "$MIN" ] && MIN="0"

    # Składanie linii w formacie <hour>:<minutes>:<command>:<info>
    LINE="${HOUR}:${MIN}:${COMMANDS[$i]}:${INFOS[$i]}"
    
    echo "$LINE" >> "$TASKFILE"
    echo "Dodano: $LINE"
done

echo "Gotowe. Możesz teraz uruchomić demona: ./dp $TASKFILE outfile"
