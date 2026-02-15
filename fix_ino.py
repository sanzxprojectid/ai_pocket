import sys

with open('AIPOCKETC3.ino', 'r') as f:
    lines = f.readlines()

# Ranges to remove (1-based, inclusive)
to_remove = [
    (1, 6), # Some includes
    (16, 18), # Screen defines
    (44, 44), # BATTERY_CRITICAL define
    (322, 322), # drawStatusBar prototype
    (393, 405), # drawBatteryIcon implementation
    (1454, 1491), # drawStatusBar implementation
    (1546, 1579) # showMainMenu implementation
]

# Sort ranges in descending order to avoid index shift issues
to_remove.sort(reverse=True)

for start, end in to_remove:
    del lines[start-1:end]

# Insert #include "globals.h" at the beginning
lines.insert(0, '#include "globals.h"\n')

with open('AIPOCKETC3.ino', 'w') as f:
    f.writelines(lines)
