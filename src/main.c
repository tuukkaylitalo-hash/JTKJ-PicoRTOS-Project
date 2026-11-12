
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



// Tehtävä 3: Tilakoneen esittely Add missing states.
// Exercise 3: Definition of the state machine. Add missing states.
// Lisätty puuttuva tila : DATA_READY
enum state { WAITING=1, DATA_READY};
enum state programState = WAITING;

// Tehtävä 3: Valoisuuden globaali muuttuja
// Exercise 3: Global variable for ambient light
uint32_t ambientLight;

// btn_fxn tunnistaa GPIO-keskeytyksen aiheuttaneen napin (SW1/SW2) ja ilmoittaa siitä print_taskille
// gpio: kertoo mikä pinni aiheutti keskeytyksen. eventMask: kertoo keskeytystyypin, jota tässä ei käytetä
// Keskeytyskäsitteljä käyttää kahvaa nappitapahtuman ilmoituksesta print_taskille

static TaskHandle_t hPrintTask = NULL;  
static void btn_fxn(uint gpio, uint32_t eventMask) {

    // Harjoitustyö: 
    if ((gpio == SW1_PIN || gpio == SW2_PIN) && hPrintTask != NULL){
        BaseType_t taskWoken = pdFALSE; // FreeRTOS-muuttuja johon merkitään, tarvitaanko tehtävän vaihtoa keskeytyksen jälkeen
        uint32_t button_event; // Keskeytyksen aiheuttaneen napin ilmoitusbitti

        if (gpio == SW1_PIN){
            button_event = SW1_EVENT;
        } else {
            button_event = SW2_EVENT;
        }

        xTaskNotifyFromISR(     // xTaskNotifyFromISR ilmoittaa keskeytyksestä print_taskille, kumpaa nappia painettiin
            hPrintTask,         // Ilmoituksen vastaanottavan tehtävän kahva
            button_event,       // SW1 tai SW2 ilmoitusbitti
            eSetBits,           // asettaa valitun bitin niin, että toisen napin käsittelemätön ilmoitus säilyy
            &taskWoken);        // FreeRTOS voi merkitä tähän tarvitaanko tehtävän vaihtoa
        
        // Jos ilmoitus herätti korkeamman prioriteetin tehtävän
        // pyydetään FreeRTOSia vaihtamaan siihen keskeytyksen päättyessä
        portYIELD_FROM_ISR(taskWoken);
    }
    
}

static void sensor_task(void *arg){
    (void)arg;
    // Tehtävä 2: Alusta valoisuusanturi. Etsi SDK-dokumentaatiosta sopiva funktio.
    // Exercise 2: Init the light sensor. Find in the SDK documentation the adequate function.
   
    // Alustetaan valoisuusanturi
    init_veml6030();
    // Annetaan ensimmäiselle mittaukselle aikaa valmistua
    vTaskDelay(pdMS_TO_TICKS(200));

    for(;;){
        
        // Tehtävä 2: Muokkaa tästä eteenpäin sovelluskoodilla. Kommentoi seuraava rivi.
        //             
        // Exercise 2: Modify with application code here. Comment following line.
        //             Read sensor data and print it out as string; 
        // tight_loop_contents(); 

        // Tehtävä 2 alla
        uint32_t light = veml6030_read_light();
        //printf("Valoisuus: %lu luxia\n", (unsigned long)light);


        // Tehtävä 3:  Muokkaa aiemmin Tehtävässä 2 tehtyä koodia ylempänä.
        //             Jos olet oikeassa tilassa, tallenna anturin arvo tulostamisen sijaan
        //             globaaliin muuttujaan.
        //             Sen jälkeen muuta tilaa.
        // Exercise 3: Modify previous code done for Exercise 2, in previous lines. 
        //             If you are in adequate state, instead of printing save the sensor value 
        //             into the global variable.
        //             After that, modify state

         // Tehtävä 3 alla

       if (programState == WAITING){
            ambientLight = veml6030_read_light();
            programState = DATA_READY;
        }
        
        // Exercise 2. Just for sanity check. Please, comment this out
        // Tehtävä 2: Just for sanity check. Please, comment this out
        // printf("sensorTask\n");

        // Do not remove this
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Print_taskissa käsitellään nappisyötöt ja IMU-mittaukset sekä lähetetään ja vastaanotetaan Morse-viestit USB-liikenteellä

static void print_task(void *arg){
    (void)arg;
    
   /* while(1){
        
        // Tehtävä 3: Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
        //            Muista tilamuutos
        //            Älä unohda kommentoida seuraavaa koodiriviä.
        // Exercise 3: Print out sensor data as string to debug window if the state is correct
        //             Remember to modify state
        //             Do not forget to comment next line of code.
        // tight_loop_contents();

        // Tehtävä 3. Tulostus kun DATA_READY (kommentoituna kun 4. tehtävä testissä)

        if(programState == DATA_READY){
            printf("Valoisuus: %lu luxia\n", (unsigned long)ambientLight);
            programState = WAITING;
        }*/

        // Tehtävä 4:n tulostus

        /*if (programState == DATA_READY) {

            // CDC0: debug-viesti
            if (usb_serial_connected()) {
                snprintf(buf, sizeof(buf), "Lux:%lu\n",
                         (unsigned long)ambientLight);

                usb_serial_print(buf);
                usb_serial_flush();
            }

            // CDC1: aikaleima ja valaistus CSV-muodossa
            if (tud_cdc_n_connected(CDC_ITF_TX)) {
                uint32_t timestamp = (uint32_t)(
                    (uint64_t)xTaskGetTickCount() *
                    1000 / configTICK_RATE_HZ
                );

                snprintf(buf, sizeof(buf), "%lu,%lu\n",
                         (unsigned long)timestamp,
                         (unsigned long)ambientLight);

                tud_cdc_n_write(
                    CDC_ITF_TX,
                    buf,
                    strlen(buf)
                );

                tud_cdc_n_write_flush(CDC_ITF_TX);
            }

            programState = WAITING;
        }*/
{
    
}
    
        // Harjoitustyön muuttujia printTaskille

        char buf[64];       // Yhdellä USB-lukukerralla vastaanotetut tavut
        char message[256];  // Useasta USB-luvusta koottava vastaanottopuskuri
        size_t length = 0;  // Vastaanottopuskuriin tallennettujen tavujen määrä
        int discard = 0;    // Arvolla 1 ohitetaan liian pitkä viesti loppumerkkiin asti
        bool writing = false; // Tarkistaa onko viestin kirjoitus käynnissä vai ei
        char tx_message[128] = "";  // Napeilla ja laiteasennoilla kirjoitettu Morse-viesti
        size_t tx_length = 0; // Viestin pituus merkkeinä, välit luetaan mukaan ja '\0' luetaan pois. 
        unsigned int sw1_streak = 0;  // Peräkkäisten SW1-painallusten "laskuri" viimeksi lisätyn symbolin jälkeen.
                                      // 1. painallus = kirjainväli, 2. = sanaväli, 3 = viesti valmis, 4 = viestin lähetys

        unsigned int symbol_count = 0; // Nykyiseen morsetukseen lisättyjen symbolien "laskuri". (4 symbolia maksimi)

        // IMU:n käyttövalmiustarkistus. Tarkistaa onko alustus ja mittauksen käynnistys onnistuneet (esmierkistä hat_imu_cdc_ex)
        bool imu_ready = (init_ICM42670() == 0) && (ICM42670_start_with_default_values() == 0);  

        // Rekisteröidään keskeytyskäsittelijä

        gpio_set_irq_enabled_with_callback(
            SW1_PIN, // GPIO-pinni
            GPIO_IRQ_EDGE_RISE, // Keskeytys nousevalla reunalla
            true, // Keskeytys käytössä
            btn_fxn); // Kutsuttava keskeytyskäsittelijä

        gpio_set_irq_enabled(
            SW2_PIN,
            GPIO_IRQ_EDGE_RISE,
            true);


        while(1){

           uint32_t button_events;  // FreeRTOS:lla luetut tapahtumabitit. Molemmat napit voivat olla ilmoitettuina

            // xTaskNotifyWait tarkistaa nappitapahtumat jäämättä odottamaan niitä

            if (xTaskNotifyWait(
                0,                      // Ilmoitusbittejä ei nollata ennen lukemista
                SW1_EVENT | SW2_EVENT,  // Kuitataan molempien nappien bitit lukemisen jälkeen
                &button_events,         // Osoite, jonne saapuneet bitit kopioidaan
                0)                      // Odotusaika (number of Ticks)
                == pdTRUE){             // == pdTRUE: ilmoitus on saatu
                
                // Saapuneiden nappitapahtumien käsittely 

                if (button_events & SW1_EVENT){
                    const char *debug_text;  // SW1-painalluksen tuloksen viesti porttiin CDC0

                    if (!writing) {
                        writing = true;
                        sw1_streak = 0; // SW1 peräkkäisten painallusten nollaus
                        symbol_count = 0; // SW2 peräkkäisten symbolien nollaus
                        debug_text = "Kirjoitus alkoi\n";
                        } else{
                            if (tx_length == 0){
                                debug_text = "Lisää ensin merkki\n";

                            } else if (sw1_streak < 2 && tx_length < sizeof(tx_message) -2){

                                tx_message[tx_length] = ' ';
                                tx_length++;
                                tx_message[tx_length] = '\0';
                                sw1_streak++;
                                symbol_count = 0;
                                debug_text = "Välilyönti\n";

                            } else if (sw1_streak == 2){

                                sw1_streak = 3;
                                debug_text = "Viesti valmis. SW1 lähettää, SW2 jatkaa. \n";

                            } else if (sw1_streak == 3){
                                // Puskurin lopussa kaksi välilyöntiä

                                tx_message[tx_length] = '\n';
                                tx_message[tx_length +1] = '\0';
                                usb_send_morse(tx_message);

                                // Tyhjennetään kirjoituspuskuri ja palataan odottamaan uutta viestiä

                                tx_length = 0;
                                tx_message[0] = '\0';
                                sw1_streak = 0;     // SW1 peräkkäisten painallusten nollaus
                                symbol_count = 0; // SW2 peräkkäisten symbolien nollaus
                                writing = false;
                                debug_text = "Lähetyspyyntö tehty\n";

                            } else {
                                debug_text = "Puskuri täynnä\n";
                            }
                            
                        }

                    if (usb_serial_connected()) {
                    usb_serial_print(debug_text);
                    usb_serial_flush();
                    }
            }

                if ((button_events & SW2_EVENT) && writing) {
                    char symbol = '\0'; //Symbolin tunnistus tähän muuttujaan sensor datan lukemisen jälkeen.

                    if(imu_ready){
                        float ax, ay, az, gx, gy, gz, t;  // ax, ay, az kiihtyvyyksiä eri akseleilla. Tunnistus käyttää X- ja Z- arvoja.

                        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0){

                            if (fabsf(az) > 0.8f){
                                symbol = '.';  // Symboli on piste kun laite on vaakatasossa kummin päin tahansa

                            } else if (fabsf(ax) > 0.8f){
                                symbol = '-';  // Symboli on viiva kun laite on kyljellään kummin päin tahansa
                            }

                        }
                    }

                    if (symbol != '\0' && 
                        tx_length < sizeof(tx_message) - 4 && // -4 varaa tilan kahdelle välilyönnille, '\n':lle ja '\0':lle
                        symbol_count < 4){
                        tx_message[tx_length] = symbol; // Annetaan symboli
                        symbol_count++;  // Kasvatetaan nykyisten kirjainten symbolimäärää
                        tx_length ++; // Kasvatetaan nykyisen viestin kokoa            
                        tx_message[tx_length] = '\0';
                        sw1_streak = 0; // Lisätty symboli katkaisee SW1-sarjan
                        }
                    

                    if (usb_serial_connected()) {
                    usb_serial_print(tx_message);
                    usb_serial_print("\n");
                    usb_serial_flush();
                }
             }
        }

            // Harjoitustyö: USB-viestien vastaanotto

            uint32_t count = usb_receive_morse(buf, sizeof(buf));  // Tällä USB-lukukerralla vastaanotettujen tavujen määrä

            for (uint32_t i = 0; i < count; i++){
                char c = buf[i];  // c on vastaanottopuskurissa käsiteltävä yksittäinen tavu, i on käsiteltävän tavun indkesi USB-lukupuskurissa

                if (c == '\n' || c == '!'){  //Huutomerkki toimii myös viestin lopettajana tässä rivinvaihdon lisäksi. Helpompi testata.
                    
                    if(!discard && ((c == '!' && length > 0) || 
                        (c == '\n' && length >= 2 &&
                        message[length - 2] == ' ' &&
                        message[length - 1] == ' '))){
                        
                        char letter = morse_to_letter("...");  // Kiinteän "..." testisyötteen purkutulos.
                                                                // Varsinaista vastaanotettua viestiä ei vielä pureta

                        if(letter == 'S'){
                            char response[] = {letter, '\n', '\0'};  // USB-testivastaus: kirjain, rivinvaihto ja merkkijonon loppu
                            usb_send_morse(response);
                        }
                        else {
                            usb_send_morse("MORSE-TESTI EPÄONNISTUI\n");
                        }
                    }
                // Aloitetaan seuraavan viestin kerääminen tyhjästä
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
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Alustetaan laite sekä USB ja luodaan FreeRTOS taskit

int main() {

    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    // Harjoitustyö
    // Alustetaan painikkeet ja LED
    init_sw1();
    init_sw2();
    init_led();

    // hSensorTask on valoisuusmittaustehtävän kahva
    // hUSB on USB-tehtävän kahva, jota käytetään tarvittaessa ydinrajaukseen

    TaskHandle_t hSensorTask, hUSB = NULL;

    // Tehtävä 4: Poista tämän xTaskCreate-rivin kommentointi luodaksesi tehtävän,
    // joka mahdollistaa kaksikanavaisen USB-viestinnän.

    // Kaksikanavaista USB-viestintää varten tehtävä, jonka FreeRTOS suorittaa ajastimen käynnistyttyä

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


    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(sensor_task, // Viimeisimmän sensor-tehtävän luonnin paluuarvo
                "sensor",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hSensorTask);                   // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Sensor task creation failed\n");
        return 0;
    }
    result = xTaskCreate(print_task,  // Viimeisimmän print-tehtävän luonnin paluuarvo
                "print",              // Tehtävän nimi vianetsintää varten 
                DEFAULT_STACK_SIZE,   // Tehtävän pinon koko sanoina, ei tavuina
                NULL,                 // Tehtäväfunktiolle annettava argumentti
                2,                    // Tehtävän prioriteetti
                &hPrintTask);         // Osoite, johon FreeRTOS tallentaa tehtävän kahvan

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    // Alusta USB-moduuli ennen kuin FreeRTOS käynnistää tehtävät

    usb_init(); // Alustaa USB-sarjaliikenteen ennen tehtävien ajastuksen käynnistämistä

    // Käynnistää FreeRTOS-ajastimen, joka alkaa suorittaa luotuja tehtäviä 
    vTaskStartScheduler();
    
    // Never reach this line.
    return 0;
}

