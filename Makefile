.PHONY: firmware firmware-build firmware-flash firmware-monitor hooks-install clean

# ESP-IDF location -- override with: make firmware-build IDF_PATH=...
IDF_EXPORT ?= $(HOME)/.espressif/v5.5.2/esp-idf/export.sh

# Default target
all: firmware-build firmware-flash

# --- Firmware ---
# Sources ESP-IDF environment automatically.
# Override IDF_EXPORT if your install is in a different location.

firmware-build:
	bash -c '. $(IDF_EXPORT) && cd firmware && idf.py build'

firmware-flash:
	bash -c '. $(IDF_EXPORT) && cd firmware && idf.py flash'

firmware-monitor:
	bash -c '. $(IDF_EXPORT) && cd firmware && idf.py monitor'

firmware-flash-monitor:
	bash -c '. $(IDF_EXPORT) && cd firmware && idf.py flash monitor'

firmware-menuconfig:
	bash -c '. $(IDF_EXPORT) && cd firmware && idf.py menuconfig'

firmware-clean:
	bash -c '. $(IDF_EXPORT) && cd firmware && idf.py fullclean'

# --- Clean ---
clean:
	$(MAKE) -C server clean
	-bash -c '. $(IDF_EXPORT) && cd firmware && idf.py fullclean' 2>/dev/null
