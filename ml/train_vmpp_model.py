import os
import sys
import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import Ridge
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score

DATASET_PATH = "../dataset/mppt_telemetry_sample.csv"
MODEL_HEADER_OUT = "../esp32_ai_mppt/model_coefficients.h"

def main():
    if not os.path.exists(DATASET_PATH):
        print(f"[ERROR] Dataset file not found at {DATASET_PATH}")
        sys.exit(1)
        
    df = pd.read_csv(DATASET_PATH)
    X = df[['temperature', 'humidity', 'cloud_cover', 'solar_radiation', 'battery_voltage']]
    y = df['vmpp']

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)

    ridge_model = Ridge(alpha=1.0)
    ridge_model.fit(X_train_scaled, y_train)
    y_pred = ridge_model.predict(X_test_scaled)

    print(f"Model Trained! MAE: {mean_absolute_error(y_test, y_pred):.4f} V | R2: {r2_score(y_test, y_pred):.4f}")

    os.makedirs("../esp32_ai_mppt", exist_ok=True)
    with open(MODEL_HEADER_OUT, "w") as f:
        f.write("#ifndef MODEL_COEFFICIENTS_H\n#define MODEL_COEFFICIENTS_H\n\n")
        f.write("const float FEATURE_MEANS[5] = {" + ", ".join([f"{val:.6f}" for val in scaler.mean_]) + "};\n")
        f.write("const float FEATURE_STDS[5] = {" + ", ".join([f"{val:.6f}" for val in scaler.scale_]) + "};\n")
        f.write("const float MODEL_WEIGHTS[5] = {" + ", ".join([f"{val:.6f}" for val in ridge_model.coef_]) + "};\n")
        f.write(f"const float MODEL_INTERCEPT = {ridge_model.intercept_:.6f};\n\n")
        f.write("inline float predict_vmpp_embedded(float temp, float humidity, float clouds, float irradiance, float bat_volts) {\n")
        f.write("    float features[5] = {temp, humidity, clouds, irradiance, bat_volts};\n")
        f.write("    float normalized[5];\n")
        f.write("    for (int i = 0; i < 5; i++) normalized[i] = (features[i] - FEATURE_MEANS[i]) / FEATURE_STDS[i];\n")
        f.write("    float predicted_vmpp = MODEL_INTERCEPT;\n")
        f.write("    for (int i = 0; i < 5; i++) predicted_vmpp += normalized[i] * MODEL_WEIGHTS[i];\n")
        f.write("    return predicted_vmpp;\n")
        f.write("}\n\n#endif\n")

    print(f"[SUCCESS] Exported C++ model header to {MODEL_HEADER_OUT}")

if __name__ == "__main__":
    main()