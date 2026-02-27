# Gemini Project Context

- The Seeed Studio XIAO ESP32-S3 Sense requires the 'PSRAM=opi' flag in arduino-cli to initialize its 8MB PSRAM and avoid camera malloc failures.
- The user (bgcl) prefers a CLI-centric development workflow where I manage coding, compilation, and flashing via terminal tools (arduino-cli, esptool, gh), while they review progress via GitHub.
- The built-in LED on the XIAO ESP32-S3 Sense is connected to GPIO 21.
