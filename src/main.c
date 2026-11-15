
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"
#include <stdbool.h>
#include "usb_comm.h" // USB-moduulin funktioita
#include "morse.h" // Kirjasto morse-merkkien muuntamiseen kirjaimiksi
#include "usbSerialDebug/helper.h"  //USB-debug kirjasto

#define DEFAULT_STACK_SIZE 2048  //Tehtävien pinon koko sanoina
#define CDC_ITF_TX      1        //CDC1-portin rajapinta

#define SW1_EVENT (1u << 0) // SW1-painalluksen ilmoitusbitti.
#define SW2_EVENT (1u << 1) // SW2-painalluksen ilmoitusbitti.

static void send_debug_output(const char *text);

enum state{ // Järjestlmän tilat
    IDLE,
    WRITING,
    READY
};
enum state programState = IDLE;

unsigned int symbol_count = 0; // Nykyiseen morsetukseen lisättyjen symbolien "laskuri". (4 symbolia maksimi)
char buf[64];       // Yhdellä USB-lukukerralla vastaanotetut tavut
char message[256];  // Useasta USB-luvusta koottava vastaanottopuskuri
size_t length = 0;  // Vastaanottopuskuriin tallennettujen tavujen määrä
int discard = 0;    // Arvolla 1 ohitetaan liian pitkä viesti loppumerkkiin asti
char tx_message[128] = "";
size_t tx_length = 0;
unsigned int sw1_streak = 0;
bool imu_ready = false;

static TaskHandle_t hMorseTask = NULL; 

static void send_debug_output(const char* text){
    if (usb_serial_connected()) {
    usb_serial_print(text);
    usb_serial_print("\n");
    usb_serial_flush();
    }
}

// TODO: USB vastaanotto

static void btn_fxn(uint gpio, uint32_t eventMask) {
    /*  Harjoitustyö: 
        Tuukka: Tässä kohdassa kysytty tekoälyltä apua sopivien FreeRTOS-funktioiden käyttöön keskeytyskäsittelijässä.
        tekoäly antoi funktiot ja niihin muutettu sopivat argumentit. Tekoälynä ChatGPT:n perusversio. */
    if ((gpio == SW1_PIN || gpio == SW2_PIN) && hMorseTask != NULL){
        BaseType_t taskWoken = pdFALSE; // FreeRTOS-muuttuja johon merkitään, tarvitaanko tehtävän vaihtoa keskeytyksen jälkeen
        uint32_t button_event; // Keskeytyksen aiheuttaneen napin ilmoitusbitti

        if (gpio == SW1_PIN){
            button_event = SW1_EVENT;
        }
        else button_event = SW2_EVENT;

        xTaskNotifyFromISR(     // xTaskNotifyFromISR ilmoittaa keskeytyksestä print_taskille, kumpaa nappia painettiin
            hMorseTask,         // Ilmoituksen vastaanottavan tehtävän kahva
            button_event,       // SW1 tai SW2 ilmoitusbitti
            eSetBits,           // asettaa valitun bitin niin, että toisen napin käsittelemätön ilmoitus säilyy
            &taskWoken);        // FreeRTOS voi merkitä tähän tarvitaanko tehtävän vaihtoa
        
        // Jos ilmoitus herätti korkeamman prioriteetin tehtävän
        // pyydetään FreeRTOSia vaihtamaan siihen keskeytyksen päättyessä
        portYIELD_FROM_ISR(taskWoken);
    }
}

static void add_symbol(){
    char symbol = '\0'; // Alustus symbolimuuttujalle.
    if(imu_ready){
        float ax, ay, az, gx, gy, gz, t;  // ax, ay, az kiihtyvyyksiä eri akseleilla. Tunnistus käyttää X- ja Z- arvoja.
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0){
            if (fabsf(ay) > 0.8f){
                symbol = '-';  // Symboli on viiva kun laite on kyljellään kummin päin tahansa
            } else symbol = '.';
        }
        if (symbol != '\0' && 
            tx_length < sizeof(tx_message) - 4 && // -4 varaa tilan kahdelle välilyönnille, '\n':lle ja '\0':lle
            symbol_count < 4)
        {
            tx_message[tx_length] = symbol; // Annetaan symboli
            symbol_count++;  // Kasvatetaan nykyisten kirjainten symbolimäärää
            tx_length ++; // Kasvatetaan nykyisen viestin kokoa            
            tx_message[tx_length] = '\0';
            sw1_streak = 0; // Lisätty symboli katkaisee SW1-sarjan
        }
        send_debug_output(tx_message);
    }
}

static void handle_sw1(){
    switch(programState){
        case IDLE:
            programState = WRITING;
            symbol_count = 0;
            send_debug_output("Kirjoitus alkoi");
            break;
        case WRITING:
            if (tx_length == 0){
                send_debug_output("Lisää ensin merkki");
            } else {
                if(sw1_streak < 2){
                    tx_message[tx_length] = ' ';
                    tx_length++;
                    tx_message[tx_length] = '\0';
                    sw1_streak++;
                    symbol_count = 0;

                    if(sw1_streak==1) send_debug_output("Kirjainväli");
                    else send_debug_output("Sanaväli");

                } else {
                    programState = READY;
                    sw1_streak = 0;
                    send_debug_output("Viesti valmis. SW1 lähettää, SW2 jatkaa.");
                }
            } 
            break;
        case READY:
            tx_message[tx_length] = '\n';
            tx_message[tx_length +1] = '\0';
            usb_send_morse(tx_message);
            // Tyhjennetään kirjoituspuskuri ja palataan odottamaan uutta viestiä
            tx_length = 0;
            tx_message[0] = '\0';
            sw1_streak = 0;
            symbol_count = 0;
            programState = IDLE;
            send_debug_output("Viesti lähetetty. Kirjoitus valmis.");
            break;
    }
}

static void handle_sw2(){
    switch(programState){
        case IDLE:
            float ax, ay, az, gx, gy, gz, t;
            char debug[128];
            ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t);
            snprintf(debug,
                sizeof(debug),
                "IMU data: ax=%.2f ay=%.2f az=%.2f gx=%.2f gy=%.2f gz=%.2f t=%.2f\n",
                ax, ay, az, gx, gy, gz, t);

            usb_serial_print(debug);
            usb_serial_flush();


            break; // Ei tehdä mitään, kun ei ole kirjoitusta käynnissä
        case WRITING:
        {
            if (symbol_count == 4){
                send_debug_output("Maksimimäärä symboleita saavutettu. SW2 lisää välilyönnin");
            }
            else add_symbol();
            break;
        }
        case READY: 
            programState = WRITING;
            sw1_streak = 0;
            send_debug_output("Kirjoitus jatkuu");
            break;
    }
}

static void morse_task(void *arg){ // Edellinen print_task
    (void)arg;

    // TODO Muuttujien alustukset

    TickType_t recent_SW1_press = 0;  // Alustetaan viimeisin SW1-painallus
    bool hasPreviousPress = false;  // Alustetaan onko vielä painallusta tullut
    TickType_t recent_SW2_press = 0;  // Alustetaan viimeisin SW2-painallus

    while(1){
        uint32_t button_events;  // FreeRTOS:lla luetut tapahtumabitit. Molemmat napit voivat olla ilmoitettuina

        if (xTaskNotifyWait(
            0,                      // Ilmoitusbittejä ei nollata ennen lukemista
            SW1_EVENT | SW2_EVENT,  // Kuitataan molempien nappien bitit lukemisen jälkeen
            &button_events,         // Osoite, jonne saapuneet bitit kopioidaan
            0)                      // Odotusaika (number of Ticks) eli ei jäädä odottamaan ilmoitusta
            == pdTRUE)
        {  
            TickType_t time_now = xTaskGetTickCount();
            if((button_events & SW1_EVENT) &&
                (!hasPreviousPress ||(time_now - recent_SW1_press >= pdMS_TO_TICKS(100))))
            {
                recent_SW1_press = time_now;
                hasPreviousPress = true;
                handle_sw1();
            }

            if((button_events & SW2_EVENT) &&
                (!hasPreviousPress ||(time_now - recent_SW2_press >= pdMS_TO_TICKS(100))))
            {
                handle_sw2();
                recent_SW2_press = time_now;
                hasPreviousPress = true;
            }
        }
    vTaskDelay(pdMS_TO_TICKS(20));
    }
}

int main(){
    // Alusta Sensor Tag
    init_hat_sdk();
    sleep_ms(300);

    // Painikkeiden ja antureiden alustukset
    init_sw1();
    init_sw2();
    init_led();
    // IMU:n käyttövalmiustarkistus. Tarkistaa onko alustus ja mittauksen käynnistys onnistuneet (esmierkistä hat_imu_cdc_ex)
    imu_ready = (init_ICM42670() == 0) && (ICM42670_start_with_default_values() == 0);  

    // Keskeytyskäsittelijät
    gpio_set_irq_enabled_with_callback(
        SW1_PIN, // GPIO-pinni
        GPIO_IRQ_EDGE_RISE, // Keskeytys nousevalla reunalla
        true, // Keskeytys käytössä
        btn_fxn); // Kutsuttava keskeytyskäsittelijä

    gpio_set_irq_enabled(
        SW2_PIN,
        GPIO_IRQ_EDGE_RISE,
        true);

    TaskHandle_t hUSB = NULL;

    BaseType_t usb_result = xTaskCreate(usb_task, // Tehtäväfunktio
                "usb",                            // Tehtävän nimi 
                2048,                             // Stackin koko
                NULL,                             // Tehtävän argumentit
                3,                                // Tehtävän prioriteetti
                &hUSB);                           // Tehtävän kahva

    if(usb_result != pdPASS){                     // USB-tehtävän luonnin paluuarvo. pdPass tarkoittaa onnistumista
        return 1;
    }

    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUSB, 1u << 0);
    #endif

    BaseType_t result = xTaskCreate(morse_task,  // Viimeisimmän print-tehtävän luonnin paluuarvo
        "morse",              // Tehtävän nimi
        DEFAULT_STACK_SIZE,   // Tehtävän pinon koko sanoina
        NULL,                 // Tehtäväfunktiolle annettava argumentti
        2,                    // Tehtävän prioriteetti
    &hMorseTask);         // Osoite, johon FreeRTOS tallentaa tehtävän kahvan

    if(result != pdPASS) {
        printf("Morse Task creation failed\n");
        return 0;
    }

    usb_init();

    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
