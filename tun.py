import numpy as np
import pandas as pd
import math

# ===== Vapor pressure =====
def vapor_pressure(t, rh):
    return (rh / 100.0) * 6.105 * math.exp((17.27 * t) / (237.7 + t))

# ===== Apparent Temperature (v = 0) =====
def apparent_temp(t, rh):
    e = vapor_pressure(t, rh)
    at = t + 0.33 * e - 4.0
    return round(at, 2)

# ===== Ranges =====
temps = np.arange(5, 45.1, 2.5)
hums  = np.arange(5, 100.1, 5)

# ===== Build table =====
data = []

for rh in hums:
    row = []
    for t in temps:
        row.append(apparent_temp(t, rh))
    data.append(row)

df = pd.DataFrame(data, columns=[f"{t:.1f}C" for t in temps])
df.insert(0, "RH%", hums)

print(df)

# Save file
df.to_csv("apparent_temperature_table.csv", index=False)