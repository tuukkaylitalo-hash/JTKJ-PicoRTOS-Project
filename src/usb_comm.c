#include "usb_comm.h" // Oman USB-moduulin funktion esittely.
#include <tusb.h>    // TinyUSB:n funktiot.
#include "usbSerialDebug/helper.h" // Debug-sarjaportin funktiot.
#include <string.h>

#define CDC_ITF_TX 1

void usb_init(void){

    // Alustetaan ensin TinyUSB, jota molemmat sarjaportit käyttävät.
    tusb_init();

    // Alustetaan debug-kirjasto CDC0-tulostusta varten.
    usb_serial_init();

}

// FreeRTOS suorittaa tämän tehtävän ohjelman käynnistyttyä.
void usb_task(void *arg) {

    // FreeRTOS voi antaa tehtävälle argumentin. Tämä tehtävä ei käytä sitä.
    (void)arg;

    // tud_task() käsittelee TinyUSB:n tapahtumia silmukassa.
    while(1) {
        tud_task();
    }
}

// Kirjoitetaan Morse-viesti CDC1-lähetyspuskuriin

void usb_send_morse(const char *message){

    if(tud_cdc_n_connected(CDC_ITF_TX)){

            tud_cdc_n_write(CDC_ITF_TX, message, strlen(message));
            tud_cdc_n_write_flush(CDC_ITF_TX);
    }

}

// Lue CDC1:stä enintään capacity tavua ja palauta luettu määrä

uint32_t usb_receive_morse(char *buffer, uint32_t capacity){

    return tud_cdc_n_read(CDC_ITF_TX, buffer, capacity);
}
