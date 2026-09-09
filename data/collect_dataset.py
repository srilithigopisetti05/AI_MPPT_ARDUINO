import os
import numpy as np
import pandas as pd

def generate_synthetic_data(samples=2000):
    np.random.seed(42)
    
    temp = np.random.uniform(10.0, 45.0, samples)
    humidity = np.random.uniform(20.0, 90.0, samples)
    cloud_cover = np.random.uniform(0.0, 100.0, samples)
    irradiance = (100.0 - cloud_cover * 0.7) * 10.0 + np.random.normal(0, 20, samples)
    irradiance = np.clip(irradiance, 50.0, 1100.0)
    
    bat_volts = np.random.uniform(11.5, 13.8, samples)
    
    v_mp_ref = 18.0
    vmpp_true = v_mp_ref + (temp - 25.0) * (-0.05) + (irradiance - 800.0) * 0.0015
    vmpp_true += np.random.normal(0, 0.2, samples)
    
    df = pd.DataFrame({
        'timestamp': pd.date_range(start='2026-01-01', periods=samples, freq='1Min'),
        'temperature': temp,
        'humidity': humidity,
        'cloud_cover': cloud_cover,
        'solar_radiation': irradiance,
        'pv_voltage': vmpp_true + np.random.normal(0, 0.1, samples),
        'pv_current': (irradiance / 1000.0) * 3.0,
        'pv_power': vmpp_true * ((irradiance / 1000.0) * 3.0),
        'battery_voltage': bat_volts,
        'pwm': np.random.randint(40, 80, samples),
        'vmpp': vmpp_true
    })
    
    os.makedirs("../dataset", exist_ok=True)
    df.to_csv("../dataset/mppt_telemetry_sample.csv", index=False)
    print("[SUCCESS] Generated training records in ../dataset/mppt_telemetry_sample.csv")

if __name__ == "__main__":
    generate_synthetic_data()