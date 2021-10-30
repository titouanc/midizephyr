/* Vertical size of the tracking zone above a distance sensor, in cm */
#define DISTANCE_SENSOR_TRACKING_ZONE_CM 60

/* Vertical size of the hysteresis to enter/leave the tracking zone, in cm */
#define DISTANCE_SENSOR_TRACKING_HYSTERESIS_CM 5

/* Alpha coefficient for the exponential moving average used to filter
 * the distance measured by the sensor, as in:
 * DISTANCE[t] = ALPHA*MEASURED_DISANTE + (1-ALPHA)*DISTANCE[t-1]
 */
#define DISTANCE_SENSOR_FILTERING_ALPHA 0.25

/* Number of distinct steps in the encoder */
#define ENCODER_N_STEPS 32
