#ifndef MODEL_COEFFICIENTS_H
#define MODEL_COEFFICIENTS_H

const float FEATURE_MEANS[5] = {27.367668, 54.352924, 49.697355, 651.121422, 12.635149};
const float FEATURE_STDS[5] = {10.211649, 19.868198, 28.774106, 202.481018, 0.649642};
const float MODEL_WEIGHTS[5] = {-0.507732, -0.000758, -0.012707, 0.290627, -0.002513};
const float MODEL_INTERCEPT = 17.657377;

inline float predict_vmpp_embedded(float temp, float humidity, float clouds, float irradiance, float bat_volts) {
    float features[5] = {temp, humidity, clouds, irradiance, bat_volts};
    float normalized[5];
    for (int i = 0; i < 5; i++) normalized[i] = (features[i] - FEATURE_MEANS[i]) / FEATURE_STDS[i];
    float predicted_vmpp = MODEL_INTERCEPT;
    for (int i = 0; i < 5; i++) predicted_vmpp += normalized[i] * MODEL_WEIGHTS[i];
    return predicted_vmpp;
}

#endif
