// Estetään tämän otsikkotiedoston sisällön käsittely kahdesti
// saman .c-tiedoston käännöksessä.
#ifndef USB_COMM_H
#define USB_COMM_H
#include <stdint.h>

// Alustetaan TinyUSB ja debug-sarjaportit.
// Projektin main() kutsuu tätä ennen FreeRTOS:in käynnistämistä
void usb_init(void);

// main() antaa tämän funktion FreeRTOS:lle xTaskCreate()-kutsussa.
void usb_task(void *arg);

// Lähetetään annettu Morse-viesti CDC1-dataporttiin, jos portti on auki
void usb_send_morse(const char *message);

// Lue CDC1:stä enintään capacity tavua puskuriin ja palauta luettu määrä.
uint32_t usb_receive_morse(char *buffer, uint32_t capacity);

#endif