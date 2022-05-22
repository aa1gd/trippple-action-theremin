int cmpfunc (const void * a, const void * b);
float measure_distance(int trigger_pin, int echo_pin, int max_range);
float measure_median_distance();
int make_interval(float distance, int divisions, int max_range);
void init_sonar();
int measure_median_interval(int trigger_pin, int echo_pin, int max_range, int divisions);