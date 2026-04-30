import numpy as np
import pandas as pd

# ===== Heat Index (NOAA approximation) =====
def heat_index(t, rh):
    return (
        -8.784695 +
        1.61139411 * t +
        2.338549 * rh -
        0.14611605 * t * rh -
        0.012308094 * t * t -
        0.016424828 * rh * rh +
        0.002211732 * t * t * rh +
        0.00072546 * t * rh * rh -
        0.000003582 * t * t * rh * rh
    )

# ===== Normalize về 0–10 =====
def comfort_index(t, rh):
    base = t

    # chỉ dùng HI khi đủ điều kiện
    if t >= 27 and rh >= 40:
        base = heat_index(t, rh)

    # mapping 10°C -> 0, 25°C -> 5, 40°C -> 10
    if base <= 10:
        ci = 0
    elif base <= 25:
        ci = 5 * (base - 10) / 15
    elif base <= 40:
        ci = 5 + 5 * (base - 25) / 15
    else:
        ci = 10

    return round(max(0, min(10, ci)), 2)


# ===== Range setup =====
temps = np.arange(5, 45.1, 2.5)   # 5 → 45
hums  = np.arange(5, 100.1, 5)    # 5 → 100

# ===== Build table =====
data = []

for rh in hums:
    row = []
    for t in temps:
        row.append(heat_index(t, rh))
    data.append(row)

df = pd.DataFrame(data, columns=[f"{t:.1f}C" for t in temps])
df.insert(0, "RH%", hums)

# ===== Output =====
print(df)

# ===== Save CSV =====
df.to_csv("heat_index.csv", index=False)