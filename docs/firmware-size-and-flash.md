# Firmware size budget and flash layout

## The 888 KiB cap

Upstream zclaw enforces an 888 KiB all-in firmware binary limit. This is an engineering discipline choice, not a hardware constraint. CI (`firmware-size-guard.yml`) blocks merges that exceed it.

The cap includes everything: zclaw app logic, ESP-IDF/FreeRTOS runtime, Wi-Fi/networking, TLS/crypto, and cert bundle.

## OTA (Over-The-Air Update)

OTA allows firmware updates over Wi-Fi without physical USB access. The device keeps two app partitions:

- **ota_0**: currently running firmware
- **ota_1**: download target for new firmware

After downloading and verifying the new image in ota_1, the device reboots into it. If boot fails, it rolls back to ota_0 automatically.

Typical use cases: remote deployment, field updates, batch device maintenance.

## Current partition table (4MB flash layout)

```
nvs,      data, nvs,     0x9000,  0x4000   (16 KiB)
nvs_key,  data, nvs_keys,0xD000,  0x1000   (4 KiB)
otadata,  data, ota,     0xE000,  0x2000   (8 KiB)
phy_init, data, phy,     0x10000, 0x1000   (4 KiB)
ota_0,    app,  ota_0,   0x20000, 0x170000 (1,472 KiB)
ota_1,    app,  ota_1,   0x190000,0x170000 (1,472 KiB)
```

Each OTA partition is 1,472 KiB. The 888 KiB limit uses ~60%, leaving 40% headroom.

## ESP32-S3 N16R8 (this fork's target)

N16R8 = **16 MB flash** + 8 MB PSRAM. The current 4MB partition table only uses ~3 MB (19%) of available flash.

### Maximum firmware size with OTA retained

| Approach | OTA partition size | Firmware limit | Notes |
|----------|-------------------|----------------|-------|
| Current (4MB layout) | 1,472 KiB | 888 KiB (artificial) | 13 MB wasted |
| Conservative | 3 MiB | ~3 MiB | ~9 MB for data partitions |
| Generous | 5 MiB | ~5 MiB | ~5 MB for data partitions |
| Maximized | 7.5 MiB | ~7.5 MiB | Almost all flash to firmware |

### Pros of relaxing the limit

- Room for more features (larger cert bundle, more tools, i18n strings, bigger prompt buffers)
- Less friction when adding functionality
- No need to optimize binary size for every change

### Cons of relaxing the limit

- Diverges from upstream CI (upstream PRs block at 888 KiB)
- Larger binary = longer flash/OTA download times
- Losing single-partition mode removes OTA rollback safety

## NVS capacity

Current NVS is 16 KiB — enough for ~100-200 short string entries. Adequate for system credentials + a few dozen `u_*` user memories.

If bulk storage is needed in the future, adding a LittleFS/SPIFFS data partition (from the unused 13 MB) is better than enlarging NVS, since NVS lacks wear-leveling optimization for heavy write loads.

## Current status (this fork)

No changes to partition table or size budget. The 888 KiB limit is not a bottleneck — current firmware is ~850 KiB with 44% of the OTA partition free. If needed, the CI guard threshold can be raised without modifying the partition table.
