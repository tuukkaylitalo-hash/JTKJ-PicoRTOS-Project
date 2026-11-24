
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pico/stdlib.h>
#include "pico/cyw43_arch.h"

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

enum state{ // Järjestlmän tilat
    IDLE,
    WRITING,
    READY
};
enum state programState = IDLE;

// Muuttujaesittelyt

unsigned int symbol_count; // Nykyiseen morsetukseen lisättyjen symbolien "laskuri". (4 symbolia maksimi)
char buf[64];       // Yhdellä USB-lukukerralla vastaanotetut tavut
char message[256];  // Useasta USB-luvusta koottava vastaanottopuskuri
char symbol;
unsigned char word_length; // Nykyisen sanan pituus
size_t length = 0;  // Vastaanottopuskuriin tallennettujen tavujen määrä
size_t old_length = 0; // Edellisen viestin pituus
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

// Set LED on/off
static void set_led(bool on) {
    gpio_put(RED_LED_PIN, on);
}

static void led_task(void *arg) {
    (void)arg;
    init_led();

    for (;;) {
        //send_debug_output("LED task running"); // sanity check
        if (programState == READY){
            //send_debug_output("LED: READY"); // sanity check
            for (int i = 0; i < tx_length; i++){
                if (tx_message[i] == '.'){
                    set_led(true);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    set_led(false);
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else if (tx_message[i] == '-'){
                    set_led(true);
                    vTaskDelay(pdMS_TO_TICKS(600));
                    set_led(false);
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else if (tx_message[i] == ' '){
                    vTaskDelay(pdMS_TO_TICKS(600));
                }
            }
        } else {
            for (int i = 0; i < old_length; i++){
                if (message[i] == '.'){
                    //send_debug_output("LED: Dot"); // sanity check
                    set_led(true);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    set_led(false);
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else if (message[i] == '-'){
                    //send_debug_output("LED: Dash"); // sanity check
                    set_led(true);
                    vTaskDelay(pdMS_TO_TICKS(600));
                    set_led(false);
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else if (message[i] == ' '){
                    //send_debug_output("LED: Space"); // sanity check
                    vTaskDelay(pdMS_TO_TICKS(600));
                }
            }
        }
    vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


static void receive_usb_task(void *arg){
    (void)arg;
    //send_debug_output("USB receive task started");
    //usb_init();
    //usb_serial_init();
    //uint32_t count = usb_receive_morse(buf, sizeof(buf));  // Tällä USB-lukukerralla vastaanotettujen tavujen määrä
    while(1){
        //send_debug_output("USB receive task running");
        uint32_t count = usb_receive_morse(buf, sizeof(buf));
        //char text[] = {count + '0', '\0'}; // Convert count to char
        //send_debug_output(text);

        for (uint32_t i = 0; i < count; i++){
            char c = buf[i];  // c on vastaanottopuskurissa käsiteltävä yksittäinen tavu, i on käsiteltävän tavun indkesi USB-lukupuskurissa
            //send_debug_output(&c); // Print the received character for debugging
            if (c == '\n' || c == '!'){  //Huutomerkki toimii myös viestin lopettajana tässä rivinvaihdon lisäksi. Helpompi testata.
                    
                if(!discard && ((c == '!' && length > 0) || 
                    (c == '\n' && length >= 2 &&
                    message[length - 2] == ' ' &&
                    message[length - 1] == ' ')))
                {
                    
                    
                    
                    /*char letter = morse_to_letter("...");  // Kiinteän "..." testisyötteen purkutulos.
                                                            // Varsinaista vastaanotettua viestiä ei vielä pureta

                    if(letter == 'S'){
                        char response[] = {letter, '\n', '\0'};  // USB-testivastaus: kirjain, rivinvaihto ja merkkijonon loppu
                        usb_send_morse(response);
                    }
                    else usb_send_morse("MORSE-TESTI EPÄONNISTUI\n");*/
                }
                // Aloitetaan seuraavan viestin kerääminen tyhjästä
                old_length = length; // Store the length of the current message before resetting
                length = 0;
                discard = 0;
                continue;
            }
            // Kerätään tavut puskuriin, kunnes viesti päättyy tai tila loppuu
            if(!discard){
                if(length < sizeof(message)){
                    message[length++] = c;
                } else {
                    discard = 1;
                }
            }
            //send_debug_output(message); // Print the current message buffer for debugging
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


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

        xTaskNotifyFromISR(     // xTaskNotifyFromISR ilmoittaa keskeytyksestä morse_taskille, kumpaa nappia painettiin
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
    symbol = '\0'; // Alustus symbolimuuttujalle.
    if(imu_ready){
        float ax, ay, az, gx, gy, gz, t;  // ax, ay, az kiihtyvyyksiä eri akseleilla. Tunnistus käyttää X- ja Z- arvoja.
        word_length++;
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0){
            if (fabsf(ay) > 0.6f){
                symbol = '.';  // Symboli on viiva kun laite on kyljellään kummin päin tahansa
            } else symbol = '-';
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

                    if(sw1_streak==1){
                        send_debug_output("Kirjainväli");
                    }
                    else{
                        send_debug_output("Sanaväli");
                        word_length = 0; // Nollataan nykyisen sanan pituus
                    }

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
            
            // Tämä on testikoodi, jolla voidaan tarkistaa IMU:n toiminta. Ei ole osa varsinaista harjoitustyötä.
            float ax, ay, az, gx, gy, gz, t;
            char debug[128];
            ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t);
            snprintf(debug,
                sizeof(debug),
                "IMU data: ax=%.2f ay=%.2f az=%.2f gx=%.2f gy=%.2f gz=%.2f t=%.2f\n",
                ax, ay, az, gx, gy, gz, t);
            usb_serial_print(debug);
            usb_serial_flush();
             
            send_debug_output("SW1 aloittaa kirjoittamisen.");
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

static void morse_task(void *arg){
    (void)arg;
    TickType_t recent_SW_press = 0;  // Alustetaan viimeisin painallus
    while(1){
        uint32_t button_events;  // FreeRTOS:lla luetut tapahtumabitit. Molemmat napit voivat olla ilmoitettuina
        xTaskNotifyWait(
            0,                      // Ilmoitusbittejä ei nollata ennen lukemista
            SW1_EVENT | SW2_EVENT,  // Kuitataan molempien nappien bitit lukemisen jälkeen
            &button_events,         // Osoite, jonne saapuneet bitit kopioidaan
            0);     // Odotusaika (number of Ticks)
        TickType_t time_now = xTaskGetTickCount();
        if((button_events & SW1_EVENT) &&
            ((time_now - recent_SW_press >= pdMS_TO_TICKS(200))))
        {
            recent_SW_press = time_now;
            handle_sw1();
        }

        if((button_events & SW2_EVENT) &&
            ((time_now - recent_SW_press >= pdMS_TO_TICKS(200))))
        {
            recent_SW_press = time_now;
            handle_sw2();
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
                2,                                // Tehtävän prioriteetti
                &hUSB);                           // Tehtävän kahva

    if(usb_result != pdPASS){                     // USB-tehtävän luonnin paluuarvo. pdPass tarkoittaa onnistumista
        return 1;
    }

BaseType_t receive_usb_result = xTaskCreate(receive_usb_task, // Tehtäväfunktio
                "receive_usb",                            // Tehtävän nimi 
                2048,                             // Stackin koko
                NULL,                             // Tehtävän argumentit
                1,                                // Tehtävän prioriteetti
                &hUSB);                           // Tehtävän kahva

    if(receive_usb_result != pdPASS){                     // USB-tehtävän luonnin paluuarvo. pdPass tarkoittaa onnistumista
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

    TaskHandle_t led_task_handle = NULL;

    BaseType_t led_result =xTaskCreate(led_task,       // (en) Task function
                "led",          // (en) Name of the task 
                512,            // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,           //(en) Arguments of the task 
                3,              // (en) Priority of this task
                &led_task_handle); // (en) A handle to control the execution of this task
    
    if(result != pdPASS) {
        printf("Morse Task creation failed\n");
        return 0;
    }

    usb_init();
    usb_serial_init();

    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
