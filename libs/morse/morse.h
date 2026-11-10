#ifndef MORSE_H
#define MORSE_H


//const char* morsetable[]; // Sisältää A-Z kirjainten morsekoodit. Jokainen kirjain on merkitty pisteillä ja viivoilla.
//size_t morsetable_size; // Määrittelee morsetaulukon koon

const char* letter_to_morse(const char* letter); // Muuttaa kirjaimen osoittimen morsekoodiksi

char morse_to_letter(const char* morse);

#endif