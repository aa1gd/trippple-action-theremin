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

enum
{
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
const uint LED_PIN = PICO_DEFAULT_LED_PIN;

void led_blinking_task(void);
void midi_write_note(uint8_t note, uint8_t volume);
void midi_turn_off_note(uint8_t note);
void midi_play_chord(struct Chord previous, struct Chord next);

int prevNote1 = 0;
int prev_interval_1 = -1;

struct Note default_note;
volatile struct Chord second, third;

const int melodic_major_scale[8] = {0, 2, 4, 5, 7, 9, 11, 12};
const int playable_natural_minor_scale[8] = {0, 2, 3, 5, 7, 8, 10, 12}; 

bool isMajorYep = true;

int main()
{
    default_note.pitch = 0;
    default_note.isSeventh = false;
    default_note.isLeadingTone = false;


    third.notes[0] = default_note;
    third.notes[1] = default_note;
    third.notes[2] = default_note;
    third.notes[3] = default_note;
    third.score = -1;

    second = third;
    board_init();
    tusb_init();

    init_sonar();

    // Setup SSD1306 display
    setup_gpios();
    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);

    while (true)
        {
            tud_task(); // tinyusb device task
            led_blinking_task();

            uint8_t packet[4];
            while ( tud_midi_available() )
                tud_midi_packet_read(packet);

            // Check for rotary switch / button inputs
            // Scan ultrasound sensors
            // float distOne = measure_median_distance();
            // int level = make_interval(distOne, 8);
            int level = measure_median_interval(8);
            
            if (level != prev_interval_1 && level != -1) // may need some editing
            {
                if (prevNote1 != 0)
                    midi_turn_off_note(prevNote1);
                
                int degree = level - PITCH_OFFSET;
                int nowNote;
                if (degree >= 0)
                {
                    if (isMajorYep)
                        nowNote = melodic_major_scale[degree] + 60;
                    else
                        nowNote = playable_natural_minor_scale[degree] + 60;
                } else
                {
                    degree += 7;
                    if (isMajorYep)
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

            // Play fake notes
            /*
            uint8_t newDist = fake_get_distance();
            if (dist1 != newDist)
            {
                dist1 = newDist;
                if (prevNote1 != 0)
                    midi_turn_off_note(prevNote1);
                midi_write_note(dist1 + 60, 127);
                prevNote1 = dist1 + 60;
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
