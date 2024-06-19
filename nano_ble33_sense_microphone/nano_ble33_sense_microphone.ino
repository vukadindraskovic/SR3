#define EIDSP_QUANTIZE_FILTERBANK   0

#include <PDM.h>
#include <sr2_inferencing.h>
#include <Arduino_LPS22HB.h>

typedef struct {
    int16_t *buffer;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

static inference_t inference;
static signed short sampleBuffer[2048];
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
bool go_recorded = true;

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    // comment out the below line to cancel the wait for USB connection (needed for native USB)
    while (!Serial);
    //Serial.println("Edge Impulse Inferencing Demo");

    // summary of inferencing settings (from model_metadata.h)
    // ei_printf("Inferencing settings:\n");
    // ei_printf("\tInterval: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
    // ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    // ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    // ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

    if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
        //ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
        return;
    }

    while (!BARO.begin()) {
        //ei_printf("Failed to initialize ppressure and temperature sensor!\n");
    }
}

void loop()
{
    //ei_printf("Starting inferencing in 2 seconds...\n");

    delay(2000);

    //ei_printf("Recording...\n");

    bool m = microphone_inference_record();
    if (!m) {
        //ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    //ei_printf("Recording done\n");

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        //ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    // print the predictions
    int prediction_index = get_prediction_index(result.classification);
    if (result.classification[prediction_index].value < 0.5) {
        //ei_printf("Impossible detection of collected data...\n");
    }
    else {
      switch (prediction_index) {
        case 0: 
          go_recorded = true;
          //ei_printf("GO: Collecting data from pin started...\n");
          break;
        case 1:
          //ei_printf("NONE: Nothing changes...\n");
          break;
        case 2:
          go_recorded = false;
          //ei_printf("STOP: Collecting data from pin paused...\n");
          break;
      }
    }

    float temp = BARO.readTemperature();
    float pressure = BARO.readPressure();

    String data = String(temp, 2) + ", " + String(pressure, 2);
    Serial.println(data);

#if EI_CLASSIFIER_HAS_ANOMALY == 1
    //ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif
}



size_t get_prediction_index(ei_impulse_result_classification_t* classification)
{
    size_t max_index = 0;
    for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
      if (classification[i].value > classification[max_index].value) {
          max_index = i;
      }
    }

    return max_index;
}

static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();

    // read into the sample buffer
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);

    if (inference.buf_ready == 0) {
        for(int i = 0; i < bytesRead>>1; i++) {
            inference.buffer[inference.buf_count++] = sampleBuffer[i];

            if(inference.buf_count >= inference.n_samples) {
                inference.buf_count = 0;
                inference.buf_ready = 1;
                break;
            }
        }
    }
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

    if(inference.buffer == NULL) {
        return false;
    }

    inference.buf_count  = 0;
    inference.n_samples  = n_samples;
    inference.buf_ready  = 0;

    // configure the data receive callback
    PDM.onReceive(&pdm_data_ready_inference_callback);

    PDM.setBufferSize(4096);

    // initialize PDM with:
    // - one channel (mono mode)
    // - a 16 kHz sample rate
    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        ei_printf("Failed to start PDM!");
        microphone_inference_end();

        return false;
    }

    // set the gain, defaults to 20
    PDM.setGain(127);

    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    inference.buf_ready = 0;
    inference.buf_count = 0;

    while(inference.buf_ready == 0) {
        delay(10);
    }

    return true;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    PDM.end();
    free(inference.buffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
