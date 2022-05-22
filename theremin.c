#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"

#include "bsp/board.h"
#include "tusb.h"

#include "chords.h"
#include "hc-sr04.h"
#include "ssd1306.h"

#define PITCH_OFFSET 0

#define DISPLAY_SDA_PIN 0
#define DISPLAY_SCL_PIN 1

#define ROTARY_1_CLK_PIN 13
#define ROTARY_1_DT_PIN 14
#define ROTARY_1_SW_PIN 15
#define ROTARY_2_CLK_PIN 5
#define ROTARY_2_DT_PIN 6
#define ROTARY_2_SW_PIN 7
#define ROTARY_3_CLK_PIN 2
#define ROTARY_3_DT_PIN 3
#define ROTARY_3_SW_PIN 4

#define SENSOR_1_TRIG_PIN 8
#define SENSOR_1_ECHO_PIN 9
#define SENSOR_2_TRIG_PIN 16
#define SENSOR_2_ECHO_PIN 17

#define MUX_1_A_PIN 18
#define MUX_1_B_PIN 19
#define MUX_1_C_PIN 20
#define MUX_2_A_PIN 10
#define MUX_2_B_PIN 11
#define MUX_2_C_PIN 12
#define MUX_2_DISABLE_PIN 21

#define FOOT_PEDAL_PIN 22

#define MAX_RANGE_1 64
#define MAX_RANGE_2 64

enum
{
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
const uint LED_PIN = PICO_DEFAULT_LED_PIN;

void setup_gpios();
void led_blinking_task(void);
void midi_write_note(uint8_t note, uint8_t volume);
void midi_turn_off_note(uint8_t note);
void midi_reset_all();
void midi_play_chord(struct Chord previous, struct Chord next);

// TODO: write code for multiplexers

void read_rotaries();
static bool prevStateCLK1;
static bool prevStateCLK2;
static bool prevStateCLK3;
static bool prevButtonState1;
static bool prevButtonState2;
static bool prevButtonState3;

// State variables for the 4 different operating modes
// Melody
static int prevMelodyNote = 0;
static int prevVolume = 0;

// Chords
static int prevInversion = 0;
static int prevDegree = 0;
volatile struct Chord second, third;

// Figured Bass
static int prevBassNote = 0;
static int prevFigBassInversion;

// Duet
static int prevDuetNote1 = 0;
static int prevDuetNote2 = 0;

static struct Note default_note; // is this necessary?

// TODO: add the other scales
static const int melodic_major_scale[8] = {0, 2, 4, 5, 7, 9, 11, 12};
static const int playable_natural_minor_scale[8] = {0, 2, 3, 5, 7, 8, 10, 12}; 

int key = 48;
bool major = true;
static int operating_mode = 0; // 0 -> melodies, 1 -> chords, 2 -> figured bass, 3 -> melody harmonization
static int midi_channel = 0; // between 0 and 15

int main()
{
    // Neccesary?
    default_note.pitch = 0;
    default_note.isSeventh = false;
    default_note.isLeadingTone = false;

    third.notes[0] = default_note;
    third.notes[1] = default_note;
    third.notes[2] = default_note;
    third.notes[3] = default_note;
    third.score = -1;

    second = third;
    // end neccessary?

    board_init();
    tusb_init();

    setup_gpios();

    // Setup SSD1306 display
    /*
    ssd1306_t disp;
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
    ssd1306_clear(&disp);
    */

    // Get initial rotary readings
    prevStateCLK1 = gpio_get(ROTARY_1_CLK_PIN);
    prevStateCLK2 = gpio_get(ROTARY_2_CLK_PIN);
    prevStateCLK3 = gpio_get(ROTARY_3_CLK_PIN);

    prevButtonState1 = gpio_get(ROTARY_1_SW_PIN); // it's high when not pressed, low when pressed
    prevButtonState2 = gpio_get(ROTARY_2_SW_PIN);
    prevButtonState3 = gpio_get(ROTARY_3_SW_PIN);

    while (true)
        {
            tud_task(); // tinyusb device task
            led_blinking_task();

            uint8_t packet[4];
            while ( tud_midi_available() )
                tud_midi_packet_read(packet);

            midi_write_note(60, 127);
            sleep_ms(1000);
            midi_turn_off_note(60);
            sleep_ms(100);
            // Check for rotary switch / button inputs
            //read_rotaries();
            /*
            if (operating_mode == 0) // single note melody mode
            {
                int level = measure_median_interval(SENSOR_2_TRIG_PIN, SENSOR_2_TRIG_PIN, MAX_RANGE_2, 8);               
                int vol = measure_median_interval(SENSOR_1_TRIG_PIN, SENSOR_1_TRIG_PIN, MAX_RANGE_1, 128);

                if (level == -1)
                {
                    if (prevMelodyNote != 0)
                    midi_turn_off_note(prevMelodyNote);
                    prevMelodyNote = 0;
                    continue;
                }

                if (vol == -1)
                    vol = 127;

                int deg = level - PITCH_OFFSET;
                int pitch = 0;

                if (deg >= 0)
                {
                    if (major)
                        pitch = melodic_major_scale[deg] + key;
                    else
                        pitch = playable_natural_minor_scale[deg] + key;
                } else
                {
                    deg += 7;
                    if (major)
                        pitch = melodic_major_scale[deg] + key - 12;
                    else
                        pitch = playable_natural_minor_scale[deg] + key - 12;
                }

                if (pitch != prevMelodyNote || vol != prevVolume)
                {
                    if (prevMelodyNote != 0)
                        midi_turn_off_note(prevMelodyNote);
                    midi_write_note(pitch, vol);
                    prevMelodyNote = pitch;
                    prevVolume = vol;
                }
            }
            */
            /*
            else if (operating_mode == 1) // chords mode
            {
                int degree = measure_median_interval(SENSOR_2_TRIG_PIN, SENSOR_2_TRIG_PIN, MAX_RANGE_2, 7);               
                int inversionLevel = measure_median_interval(SENSOR_1_TRIG_PIN, SENSOR_1_TRIG_PIN, MAX_RANGE_1, 7);

                int inversion = inversionConversion(inversionLevel);
                // check foot pedal here
            }
            else if (operating_mode == 2) // figured bass mode
            {

            }
            else if (operating_mode == 3) // duet mode
            {

            }
            else // something went wrong
            {
                return 1;
            }
            */

            /*
            // Scan ultrasound sensors
            int level = measure_median_interval(SENSOR_1_TRIG_PIN, SENSOR_1_TRIG_PIN, MAX_RANGE_1, 8);
            
            if (level != prev_interval_1 && level != -1) // may need some editing
            {
                if (prevNote1 != 0)
                    midi_turn_off_note(prevNote1);
                
                int degree = level - PITCH_OFFSET;
                int nowNote;
                if (degree >= 0)
                {
                    if (major)
                        nowNote = melodic_major_scale[degree] + 60;
                    else
                        nowNote = playable_natural_minor_scale[degree] + 60;
                } else
                {
                    degree += 7;
                    if (major)
                        nowNote = melodic_major_scale[degree] + 48;
                    else
                        nowNote = playable_natural_minor_scale[degree] + 48;
                }
                
                midi_write_note(nowNote, 127); // LOUDEST VOLUME
                prevNote1 = nowNote;
                prev_interval_1 = level;
            } else if (level == -1)
            {
                midi_turn_off_note(prevNote1);
                prev_interval_1 = -1;
                prevNote1 = 0;
            }
            */

            /*
            if (level != prev_interval_1 && level != -1 && level > 6) // may need some editing
            {
                genChord(level + 1, 0, second, &third);
                midi_play_chord(second, third);
                second = third;
                    
            } else if (level == -1)
            {
                prev_interval_1 = -1;
                for (int i = 20; i < 120; i++){
                    midi_turn_off_note(i);
                }
            }
            */

            // Play fake chords
            // bool successful_gen
            /*
            if (fake_chord_gen()) 
            {
                midi_play_chord(second, third);
                //first = second;
                second = third;
            }
            */
            
            // Check for foot pedal
            // Play note / chords
        }
    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

void midi_write_note(uint8_t note, uint8_t volume)
{
    uint8_t const cable_num = 0;
    uint8_t const channel = 0;

    uint8_t note_on[3] = { 0x90 | channel, note, volume};
    tud_midi_stream_write(cable_num, note_on, 3);

}

void midi_turn_off_note(uint8_t note)
{ 
    uint8_t const cable_num = 0;
    uint8_t const channel = 0;

    uint8_t note_off[3] = { 0x80 | channel, note, 0};
    tud_midi_stream_write(cable_num, note_off, 3);
}

void midi_reset_all(){
    for (int i = 0; i < 128; i++)
    {
        midi_turn_off_note(i);
    }
}

void midi_play_chord(struct Chord previous, struct Chord next)
{
    for (int i = 0; i < 4; i++)
        {
            // if (previous.notes[i].pitch > 0 && previous.notes[i].pitch < 128)
            midi_turn_off_note(previous.notes[i].pitch);
        }

    for (int i = 0; i < 4; i++)
        {
            midi_write_note(next.notes[i].pitch, 127);
        }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // Blink every interval ms
    if (board_millis() - start_ms < blink_interval_ms)
        return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
}

void setup_gpios()
{
    //setup_ssd_gpios();

    gpio_init(ROTARY_1_CLK_PIN);
    gpio_init(ROTARY_1_DT_PIN);
    gpio_init(ROTARY_1_SW_PIN);
    gpio_init(ROTARY_2_CLK_PIN);
    gpio_init(ROTARY_2_DT_PIN);
    gpio_init(ROTARY_2_SW_PIN);
    gpio_init(ROTARY_3_CLK_PIN);
    gpio_init(ROTARY_3_DT_PIN);
    gpio_init(ROTARY_3_SW_PIN);

    gpio_set_dir(ROTARY_1_CLK_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_1_DT_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_1_SW_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_2_CLK_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_2_DT_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_2_SW_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_3_CLK_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_3_DT_PIN, GPIO_IN);
    gpio_set_dir(ROTARY_3_SW_PIN, GPIO_IN);

    init_sonar();

    gpio_init(MUX_1_A_PIN);
    gpio_init(MUX_1_B_PIN);
    gpio_init(MUX_1_C_PIN);
    gpio_init(MUX_2_A_PIN);
    gpio_init(MUX_2_B_PIN);
    gpio_init(MUX_2_C_PIN);
    gpio_init(MUX_2_DISABLE_PIN);

    gpio_set_dir(MUX_1_A_PIN, GPIO_OUT);
    gpio_set_dir(MUX_1_B_PIN, GPIO_OUT);
    gpio_set_dir(MUX_1_C_PIN, GPIO_OUT);
    gpio_set_dir(MUX_2_A_PIN, GPIO_OUT);
    gpio_set_dir(MUX_2_B_PIN, GPIO_OUT);
    gpio_set_dir(MUX_2_C_PIN, GPIO_OUT);
    gpio_set_dir(MUX_2_DISABLE_PIN, GPIO_OUT);

    gpio_init(FOOT_PEDAL_PIN);
    gpio_set_dir(FOOT_PEDAL_PIN, GPIO_IN);
}

// TODO: add the protection for ++ and --
void read_rotaries()
{
    // Read the first encoder
    bool currentStateCLK = gpio_get(ROTARY_1_CLK_PIN);

    if (currentStateCLK != prevStateCLK1 && currentStateCLK == false)
    {
        if (gpio_get(ROTARY_1_DT_PIN) == currentStateCLK)
        {
            // Counterclockwise
            key--;
        }
        else 
        {
            key++;
        }
    }
    prevStateCLK1 = currentStateCLK;

    bool currentStateSW = gpio_get(ROTARY_1_SW_PIN);

    if (!currentStateSW && prevButtonState1)
        major = !major;
    
    prevButtonState1 = currentStateSW;

    // Now read the second encoder
    currentStateCLK = gpio_get(ROTARY_2_CLK_PIN);

    if (currentStateCLK != prevStateCLK2 && currentStateCLK == false)
    {
        if (gpio_get(ROTARY_2_DT_PIN) != currentStateCLK)
        {
            // Counterclockwise
            operating_mode--;
        }
        else 
        {
            operating_mode++;
        }
    }

    prevStateCLK2 = currentStateCLK;

    currentStateSW = gpio_get(ROTARY_2_SW_PIN);

    if (!currentStateSW && prevButtonState2)
        midi_reset_all();

    prevButtonState2 = currentStateSW;


    // Finally, read the third encoder
    currentStateCLK = gpio_get(ROTARY_3_CLK_PIN);

    if (currentStateCLK != prevStateCLK3 && currentStateCLK == false)
    {
        if (gpio_get(ROTARY_3_DT_PIN) != currentStateCLK)
        {
            // Counterclockwise
            midi_channel--;
        }
        else 
        {
            midi_channel++;
        }
    }

    prevStateCLK3 = currentStateCLK;

    currentStateSW = gpio_get(ROTARY_3_SW_PIN);

    if (!currentStateSW && prevButtonState3)
        ;

    prevButtonState3 = currentStateSW;
}