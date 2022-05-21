#include <stdlib.h>
#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "pico/types.h"

#include "hc-sr04.h"

#define TRIGGER_PIN 17
#define ECHO_PIN 16

#define MEDIAN_NUM_SAMPLES 5

const uint MAX_RANGE = 64; // in cm

// used for qsort
// TODO this loses precision because it's casting to int...
int cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

void init_sonar()
{
    // setup the trigger pin in putput mode and echo pin to input mode
    gpio_init( TRIGGER_PIN );
    gpio_init( ECHO_PIN );
    gpio_set_dir( TRIGGER_PIN, GPIO_OUT );
    gpio_set_dir( ECHO_PIN, GPIO_IN );
}

float measure_distance(){
    float distance_cm = 0.0;

    gpio_set_input_enabled( ECHO_PIN, 1 );
    gpio_put( TRIGGER_PIN, 0 ); // make sure that the pin is low
    sleep_ms(2);                // wait a bit to let things stabalize
    uint echo_pin = gpio_get( ECHO_PIN);
	// printf("\nPulsed the pin high echo pin is %d", echo_pin );
	
    gpio_put( TRIGGER_PIN, 1);  // go high
    sleep_ms( 10 );             // sleep 3 mili seconds
    gpio_put( TRIGGER_PIN, 0);  // Switch the trigger low again to let the SR04 send the pulse
    
	/*
		The echo pin outputs a pulse between 150 micro-seconds and 25 mili-seconds, or if no object is found, it will send a 38 ms pulse. 
		speed of sound is 343 meters per second. This would depend upon the elevation and hummidity, but 
		sufficiently accurate for our application. 

		speed = distance travelled / time taken
		
		so, distance = (speed * time taken)/2 -- Divide by 2 because we are listening to the echo
		
		Now, speed of sound is 343 meters/second, whch is 3.43 centimeters per second, which is
		3.43/ 1000000 = 0.0343 cm/microsecond. 
		
		
	*/
    absolute_time_t  listen_start_time = get_absolute_time();
    absolute_time_t  max_wait_time  = delayed_by_ms( listen_start_time, 30 );  // No point waiting more than 30 Mili sec. 
	
    while (true)
    {
        absolute_time_t  t_now = get_absolute_time();
        if( t_now._private_us_since_boot > max_wait_time._private_us_since_boot ){
            break;
        }
        echo_pin = gpio_get( ECHO_PIN);
        if( echo_pin != 0 ){   // We got an echo!
            absolute_time_t  first_echo_time = t_now;
            while( echo_pin == 1 && t_now._private_us_since_boot < max_wait_time._private_us_since_boot ){
                echo_pin = gpio_get( ECHO_PIN );
                t_now = get_absolute_time();
            }
			if( echo_pin == 1 ){
				break;	// will return 0 cm.
			}
            int64_t pulse_high_time = absolute_time_diff_us( first_echo_time, t_now );
			float pulse_high_time_float = (float) pulse_high_time;
            distance_cm = pulse_high_time_float * 0.01715; //pulse_high_time_float / 58.0 ;
            break;
        }
    }
    gpio_set_input_enabled( ECHO_PIN, 0 );
    return distance_cm;
}

float measure_median_distance()
{
    float distances[MEDIAN_NUM_SAMPLES];

    for (int i = 0; i < MEDIAN_NUM_SAMPLES; i++)
    {
        distances[i] = measure_distance();
    }

    qsort(distances, MEDIAN_NUM_SAMPLES, sizeof(float), cmpfunc);
    return distances[MEDIAN_NUM_SAMPLES / 2];
}

int make_interval(float distance, int divisions)
{
    if (distance == 0.0)
        return -1;
    int gap_dist = MAX_RANGE / divisions;

    int interval = (int)distance; // make sure MAX_RANGE is a multiple of divisions
    //printf("gap dist %d and typecasted dist %d\n", gap_dist, interval);

    interval /= gap_dist;

    //printf("now interval %d\n", interval);

    // low sol to high do is 11 divisions (sol, la, ti, do, re, mi, fa, sol, la, ti, do)
    // inversions are 7 (0, 6, 64, 7, 65, 43, 42)
    if (interval >= divisions)
        return -1;
    return interval;
}

int measure_median_interval(int divisions)
{
    int sorted_intervals[MEDIAN_NUM_SAMPLES];

    for (int i = 0; i < MEDIAN_NUM_SAMPLES; i++)
    {
        sorted_intervals[i] = make_interval(measure_distance(), divisions);
    }

    qsort(sorted_intervals, MEDIAN_NUM_SAMPLES, sizeof(float), cmpfunc);
    return sorted_intervals[MEDIAN_NUM_SAMPLES / 2];
}