# sim/

SimulIDE circuit for the DHT demo.

> **Placeholder.** No `demo.sim1` is checked in yet; follow the steps below once
> on a Windows machine with SimulIDE installed, then save the circuit here.

## Setup (one-time, Windows)

1. Build the firmware:

   ```powershell
   .\Build.ps1 -Mcu atmega328p -OutDir build\sim
   ```

2. Open SimulIDE and build the circuit:
   - **MCU**: ATmega328P
   - **Sensor**: DHT22 (SimulIDE *Perifericals → Sensors → DHT22*) connected to **PD2**
     with a 4.7 kΩ pull-up to VDD. Use the DHT11 model if `DHT_TYPE` is set to
     `DHT_TYPE_DHT11`.
   - **UART monitor**: SerialTerm on **PD1 (TXD)**, 9600 baud, 8N1.
3. Right-click the MCU → *Load firmware* → `build\sim\demo.hex`.
4. Save the circuit as `sim\demo.sim1`.

Once the file exists, `.\Simulate.ps1` builds and opens it automatically.

## Expected behavior

Every ~2 s the UART monitor prints a line like:

```
T=25.7 C  H=65.8 %
```

Change the sensor's temperature or humidity sliders in SimulIDE to see the
output follow.
