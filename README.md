# Project Cemetery
Putting sensors around the grave to monitor plants

# Origin
My grandparents, on my fathers side, died when I was about 5 or 6 years old. It is amazing to see that in this short period of time the interests and engineering skills of my grandfather transferred over to me.

They're buried in my city, the cemetery has free wifi which covers to entire grounds and I'm learning C++ & Python on these awesome IoT devices soooo 1+1=3

I can honor my grandfather and keep an eye on the plants on their grave by placing a fully self-contained soil moisture sensor to notify us on how the plantlife is doing.

# Hardware
The device is a Particle Photon with
- SHT31 Temperature-Humidity Sensor
- TSL2561 Lux/Luminosity Sensor
- Sparkfun Soil Moisture Sensor
- Sparkfun Battery Babysitter
- Solar panel 200mA
- LiPo 2500 mAh

# Features
The main function is to provide information on soil moisture levels, a total of 6 dweets will be sent:
- Soil Moisture level
- Temperature
- Humidity
- Light level
- LiPo State Of Charge
- Sleep Interval

The device must be self-contained meaning there is no external power. The onboard LiPo must be enough to cover the night and/or cloudy days. The solar panel must be able to charge the LiPo and provide the primary source of daytime power.

When power levels drop, the device must increase the time between measurements and transmit cycle to save power. Larger increase when charge cycles are not met.
In the offtime the device must enter deepsleep to save more power.

# Deep Sleep table
Measurements under normal conditions should be twice a day.

| LiPo capacity remaining | Calc                 | DeepSleep time  |
|-------------------------|----------------------|-----------------|
| 100%                    | (105-100)*8640=43200 | 12 hours        |
| 75%                     | (105-75)*8640=259200 | 3 days          |
| 50%                     | (105-50)*8640=475200 | 5 days 12 hours |
| 25%                     | (105-25)*8640=691200 | 8 days          |
| 10%                     | (105-10)*8640=820800 | 9 days 12 hours |

(calc still need to account for solar panel dark times such as cloudy days and night to increase the DeepSleep interval)
