# ESP 32 Smart watch

## Key Features

### 🌍 Geo-Location Time Sync
At startup, the watch fetches current Unix epoch time, time zone, daylight saving time, latitude, longitude, and city name using IP geo-location. It sets the RTC and displays:
- Unix Epoch Time
- Latitude and Longitude
- City Name
This data is fetched from an API for real-time accuracy—nothing is hard-coded.

### 🗓️ Date, Time, and Day Display
Displays the current day, date, month, and time on the main screen. Info is continuously updated so you’re always seeing the correct data.

### ⏰ Recurring Alarm
You can set a recurring alarm using the buttons on the watch. Once set, the alarm time is shown on the main screen and will go off daily.

### 🔕 Alarm Snooze and Mute
- **Snooze:** Delay the alarm by 5 minutes with the snooze feature.
- **Mute:** If you're not ready, you can mute the alarm, and it will stop after 10 minutes automatically.

### 🌡️ Room Temperature and Air Pressure
A dedicated screen shows real-time room temperature and air pressure. Data refreshes regularly for up-to-date info.

### ☁️ Weather Forecast
This feature fetches the latest weather forecast based on your location. The data refreshes every 15 minutes to ensure it's always accurate.

### 🗺️ Manual Location Input
In case geo-location isn't precise enough, you can manually input a city name or latitude/longitude for more accurate weather data.

### 📰 News Feed
Stay informed with the latest news! The watch displays top news headlines from 3 selectable sources (e.g., BBC, CNN). Headlines scroll across the screen for easy reading.

## What I Learned

This project helped me deepen my understanding of:
- Microcontroller programming with Mbed OS
- Working with APIs for real-time data retrieval
- Integrating hardware features like temperature sensors and displays
- Designing user-friendly interfaces for embedded systems

## Technologies Used
- **Mbed OS** for the microcontroller
- **C++** for code implementation
- **APIs** for geo-location, weather data, and news feeds
- **Sensors** for temperature and air pressure readings

## Hardware Requirements
- Microcontroller with Mbed OS
- 16x2 Display
- Piezo Speaker
- At least 3 Buttons
- Temperature and Air Pressure Sensor (optional)
