using json = nlohmann::json;

InterruptIn NEXT(PC_0, PullUp);
InterruptIn PREV(PC_2, PullUp);

// this three buttons is used for mutliple different thinks
InterruptIn buttonAction1(PC_3, PullUp); // this button increases latitude, increases alarm minutes and  mutes the alarm
InterruptIn buttonAction2(PC_4, PullUp); // this button increases longitude, increases alarm hours and snoozes the alarm
InterruptIn buttonAction3(PC_5, PullUp);  // this button changes increment value of latitude and longitude and Enables/disables the alarm


DigitalOut led1(LED1);
DFRobot_RGBLCD lcd(16, 2, PB_9, PB_8);
DevI2C i2c(PB_11, PB_10);
HTS221Sensor sensor(&i2c);
PwmOut alarm(PA_15);

int ScreenIndex = 0;
volatile bool ISNEXT = false;
volatile bool ISPREV = false;
volatile bool DisplayRunning = true;


int epochTime;
std::string timezone;
int timezone_offset;
bool DaylightSavingTime;
float ip_latitude;
float ip_longitude;
std::string city;

std::string weather_description;
float weather_temp;
float weather_pressure;
volatile float weather_latitude;
volatile float weather_longitude;

// alarm_hours and alarm_minutes is 0 meaning the default alarm time if not changed is 00:00
volatile int alarm_hours = 13;
volatile int alarm_minutes = 29;
volatile int alarm_hours_Snoozed = 0;
volatile int alarm_minutes_Snoozed = 0;
volatile int SnoozedCountPressed = 0;
int SnoozedCount = 1; // change this number to change how many times you can press the snooze button
volatile bool IsAlarmEnabled = false; // Default is false meaning you need to enable the alarm
volatile bool AlarmIsSounding = false; 
volatile bool IsAlarmMuted = false;
volatile bool IsAlarmSnoozed = false;
Thread alarmThread;
Timer AlarmDuration;

std::string NewsSrc;
std::string News1;
std::string News2;
std::string News3;

// Get pointer to default network interface
NetworkInterface *network = NetworkInterface::get_default_instance();

const char* apiKeyStartup = "apiKeyStartup";
const char* apiKeyWeather = "apiKeyWeather";

void connect_network() {
 
    if (!network) {
        printf("Failed to get default network interface\n");
        while (true);
    }

    nsapi_size_or_error_t result;

    do {
        printf("Connecting to the network...\n");
        result = network->connect();

        if (result != NSAPI_ERROR_OK) {
            printf("Failed to connect to network: %d\n", result);
        }
    } while (result != NSAPI_ERROR_OK);

    printf("Successfully connected to the network...\n");

}

void UNIXDATA() {

    nsapi_size_or_error_t result;

    TLSSocket socket;
    socket.set_root_ca_cert(ca_cert);

    result = socket.open(network);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to open socket: %d\n", result);
        return;
    }
    
    // Make a GET request to the API
    char request[512];
    snprintf(request, sizeof(request),
            "GET /timezone?apiKey=%s HTTP/1.1\r\n"
            "Host: api.ipgeolocation.io\r\n"
            "Connection: close\r\n"
            "\r\n",apiKeyStartup);

    SocketAddress address;
    result = network->gethostbyname("ipgeolocation.io", &address);


  // Checking the result
  if (result != 0) {
    printf("Failed to get IP address of host: %d\n", result);
    while (true);
  }
    
    address.set_port(443);
    result = socket.connect(address);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to connect to server: %d\n", result);
        socket.close();
        return;
    }
    
    printf("Succeeded to connect to server\n");

    result = socket.send(request, strlen(request)); // Send the request
    if (result < 0) {
        printf("Failed to send request: %d\n", result);
        socket.close();
        return;
    }

    printf("Succeeded to send request\n");
    char response[3000];
    result = socket.recv(response, sizeof(response)); // Receive the response
    if (result < 0) {
        printf("Failed to receive response: %d\n", result);
        socket.close();
        return;
    }
    printf("Succeeded to receive request\n");

    response[result] = '\0'; // Null-terminate the response string

    char *json_begin = strchr(response, '{');
    char *json_end = strrchr(response, '}'); 

    // Check if we actually got JSON in the response
    if (json_begin == nullptr || json_end == nullptr) {
      printf("Failed to find JSON in response\n");
      socket.close();
      return;
    }

    // End the string after the end of the JSON data in case the response contains trailing data
    json_end[1] = 0;

    printf("JSON response:\n%s\n", json_begin);

    // Parse response as JSON, starting from the first {
    json document = json::parse(json_begin);

    // Get the required information from the JSON
    timezone = document["timezone"];
    timezone_offset = document["timezone_offset"];
    DaylightSavingTime = document["is_dst"];
    epochTime = document["date_time_unix"];
    ip_latitude = std::stod(document["geo"]["latitude"].get<std::string>());
    ip_longitude = std::stod(document["geo"]["longitude"].get<std::string>());
    city = document["geo"]["city"];

    printf("Succeeded to parse the request\n\n");
    // Display the information on the LCD for 2 seconds
    lcd.printf("Unix epoch time:");
    lcd.setCursor(0, 1);
    lcd.printf("%llu", epochTime);
    ThisThread::sleep_for(2000ms);
    lcd.clear();
    lcd.printf("Lat: %.6f", ip_latitude);
    lcd.setCursor(0, 1);
    lcd.printf("Lon: %.6f", ip_longitude);
    ThisThread::sleep_for(2000ms);
    lcd.clear();
    lcd.printf("City:");
    lcd.setCursor(0, 1);
    lcd.printf("%s", city.c_str());
    ThisThread::sleep_for(2000ms);
    lcd.clear();
    socket.close();

    // Sets the weather lat&lon to ip lat&lon. This is tempeorary and can be changed later by the user
    weather_latitude = ip_latitude;
    weather_longitude = ip_longitude;

}


void Get_Weather() {
    nsapi_size_or_error_t result;
    TCPSocket socket;
    socket.set_timeout(1000);

    result = socket.open(network);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to open socket: %d\n", result);
        return;
    }

    // Make a GET request to the API
    char request[512];
    snprintf(request, sizeof(request),
            "GET /v1/current.json?key=%s&q=%f,%f&aqi=no HTTP/1.1\r\n"
            "Host: api.weatherapi.com\r\n"
            "Connection: close\r\n"
            "\r\n",apiKeyWeather,weather_latitude,weather_longitude);

    SocketAddress address;
    result = network->gethostbyname("api.weatherapi.com", &address);

    // Checking the result
    if (result != 0) {
        printf("Failed to get IP address of host: %d\n", result);
        while (true);
    }

    address.set_port(80);
    result = socket.connect(address);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to connect to server: %d\n", result);
        socket.close();
        return;
    }

    printf("Succeeded to connect to server\n");

    result = socket.send(request, strlen(request)); // Send the request
    if (result < 0) {
        printf("Failed to send request: %d\n", result);
        socket.close();
        return;
    }
    printf("Succeeded to send request\n");

    char response[3000];
    result = read_response(&socket, response, sizeof(response)); // Receive the response

    if (result < 0) {
        printf("Failed to receive response: %d\n", result);
        socket.close();
        return;
    }

    printf("Succeeded to receive request\n");

    response[result] = '\0'; // Null-terminate the response string
    printf("Response: %s", response);

    char *json_begin = strchr(response, '{');
    char *json_end = strrchr(response, '}');

    printf("Response:\n%s\n", response);

    // Check if we actually got JSON in the response
    if (json_begin == nullptr || json_end == nullptr) {
        printf("Failed to find JSON in response\n");
        socket.close();
        return;
    }

    // End the string after the end of the JSON data in case the response contains trailing data
    json_end[1] = 0;

    // Parse response as JSON
    json document = json::parse(json_begin);

    // Get the required information from the JSON
    weather_description = document["current"]["condition"]["text"];
    weather_temp = document["current"]["temp_c"];
    weather_pressure = document["current"]["pressure_mb"];

    printf("Succeeded to parse the request\n");
}


void DisplayWeather() {

    DisplayRunning = true;
    // Gets the weather information when user selected the DisplayWeather screen
    Get_Weather();
    // Prints the information from Get_Weather to the screen
    lcd.printf("%s", weather_description.c_str());
    lcd.setCursor(0, 1);
    lcd.printf("%.0f degrees", weather_temp);

    // Takes the time and stores it StartTime
    time_t StartTime = time(NULL); 

    // While loop that gets the current time and updates the lcd after 15 minutes
    while (DisplayRunning) {
        time_t currentTime = time(NULL); 
        if ((currentTime-StartTime) == (60*15)) { // Checks if 15 minutes have passed
            Get_Weather(); // Gets the weather information when 15 minutes have passed
            StartTime = time(NULL); // Resets the start time
            // Clears the lcd and prints the newly gotten information from Get_Weather to the screen 
            lcd.clear();
            lcd.printf("%s", weather_description.c_str());
            lcd.setCursor(0, 1);
            lcd.printf("%.0f degrees", weather_temp);
        }
    }
}

void WeatherLatIncrease() {

        weather_latitude += 1; // Increases the latitude with 1
    
    if (weather_latitude > 90) { // if max latitude is reached it will wrap around
            weather_latitude = -90;
        }
}

void WeatherLonIncrease() {

        weather_longitude += 1; // Increases the longitude with 1

    if (weather_longitude > 180) { // if max longitude is reached it will wrap around
            weather_longitude = -180;
        }
    }



void DisplayLatLon() {


    DisplayRunning = true;

    while (DisplayRunning) {
        buttonAction1.fall(&WeatherLatIncrease);
        buttonAction2.fall(&WeatherLonIncrease);
        lcd.clear();
        lcd.printf("Lat: %0.2f", weather_latitude);
        lcd.setCursor(0, 1);
        lcd.printf("Lon: %0.2f ", weather_longitude); 
        ThisThread::sleep_for(1000ms);
    }
    
}

void Get_News_3() {

    nsapi_size_or_error_t result;

    TCPSocket socket;

    result = socket.open(network);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to open socket: %d\n", result);
        return;
    }
   
    char request[512];
    snprintf(request, sizeof(request),
            "GET /news/world/rss HTTP/1.1\r\n"
            "Host: www.independent.co.uk\r\n"
            "Connection: close\r\n"
            "\r\n");

    SocketAddress address;
    result = network->gethostbyname("www.independent.co.uk", &address);


  // Checking the result
  if (result != 0) {
    printf("Failed to get IP address of host: %d\n", result);
    while (true);
  }
    
    address.set_port(80);
    result = socket.connect(address);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to connect to server: %d\n", result);
        socket.close();
        return;
    }
    
    printf("Succeeded to connect to server\n");

    result = socket.send(request, strlen(request)); // Send the request
    if (result < 0) {
        printf("Failed to send request: %d\n", result);
        socket.close();
        return;
    }
    
    printf("Succeeded to send request\n");
    
    
    char response[6000];

    result = read_response(&socket, response, sizeof(response));// Receive the response
    if (result < 0) {
        printf("Failed to receive response: %d\n", result);
        socket.close();
        return;
    }

    printf("Succeeded to receive request\n");

    response[result] = '\0'; // Null-terminate the response string


    //printf("Response: %s\n",response);

    char *headline_begin = strchr(response, '<title><![CDATA[');
    char *headline_end = strchr(response, ']]></title>'); 

    // Check if we actually got any headline in the response
    if (headline_begin == nullptr || headline_end == nullptr) {
      printf("Failed to find headline in response\n");
      socket.close();
      return;
    }

    // Get the required information from the xml

    // This extracts the source the feed
    char* title_start = strstr(response, "<title>");
    char* title_end = nullptr;
    if (title_start != nullptr) {
        title_start += strlen("<title>");
        title_end = strstr(title_start, "</title>");
        if (title_end != nullptr) {
            size_t src_length = title_end - title_start;
            NewsSrc = std::string(title_start, src_length);
        }
    }

    // This extracts the top 3 news.
    int count = 0;
    char* item_start = strstr(response, "<item>");
    while (item_start != nullptr && count < 3) {
        char* item_title_start = strstr(item_start, "<title><![CDATA[");
        if (item_title_start != nullptr) {
            item_title_start += strlen("<title><![CDATA[");
            char* item_title_end = strstr(item_title_start, "]]></title>");
            if (item_title_end != nullptr) {
                size_t title_length = item_title_end - item_title_start;
                std::string news_title(item_title_start, title_length);

                switch (count) {
                    case 0:
                        News1 = news_title;
                        break;
                    case 1:
                        News2 = news_title;
                        break;
                    case 2:
                        News3 = news_title;
                        break;
                }

                ++count;
            }
        }

        item_start = strstr(item_start + strlen("<item>"), "<item>");
    }

    //printf("%s",response);
    printf("\nNews7: %s\n",News1.c_str());
    printf("News8: %s\n",News2.c_str());
    printf("News9: %s\n",News3.c_str());
    printf("Source3: %s",NewsSrc.c_str());
    socket.close();
}

void Get_News_2() {

    nsapi_size_or_error_t result;

    TCPSocket socket;

    result = socket.open(network);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to open socket: %d\n", result);
        return;
    }
   
    char request[512];
    snprintf(request, sizeof(request),
            "GET /rss/edition.rss HTTP/1.1\r\n"
            "Host: rss.cnn.com \r\n"
            "Connection: close\r\n"
            "\r\n");

    SocketAddress address;
    result = network->gethostbyname("rss.cnn.com", &address);


  // Checking the result
  if (result != 0) {
    printf("Failed to get IP address of host: %d\n", result);
    while (true);
  }
    
    address.set_port(80);
    result = socket.connect(address);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to connect to server: %d\n", result);
        socket.close();
        return;
    }
    
    printf("Succeeded to connect to server\n");

    result = socket.send(request, strlen(request)); // Send the request
    if (result < 0) {
        printf("Failed to send request: %d\n", result);
        socket.close();
        return;
    }
    
    printf("Succeeded to send request\n");
    
    
    char response[6000];

    result = read_response(&socket, response, sizeof(response));// Receive the response
    if (result < 0) {
        printf("Failed to receive response: %d\n", result);
        socket.close();
        return;
    }

    printf("Succeeded to receive request\n");

    response[result] = '\0'; // Null-terminate the response string


    //printf("Response: %s\n",response);

    char *headline_begin = strchr(response, '<title><![CDATA[');
    char *headline_end = strchr(response, ']]></title>'); 

    // Check if we actually got any headline in the response
    if (headline_begin == nullptr || headline_end == nullptr) {
      printf("Failed to find headline in response\n");
      socket.close();
      return;
    }

    // Get the required information from the xml

    // This extracts the source the feed
    char* title_start = strstr(response, "<title><![CDATA[");
    char* title_end = nullptr;
    if (title_start != nullptr) {
        title_start += strlen("<title><![CDATA[");
        title_end = strstr(title_start, "]]></title>");
        if (title_end != nullptr) {
            size_t src_length = title_end - title_start;
            NewsSrc = std::string(title_start, src_length);
        }
    }

    // This extracts the top 3 news.
    int count = 0;
    char* item_start = strstr(response, "<item>");
    while (item_start != nullptr && count < 3) {
        char* item_title_start = strstr(item_start, "<title><![CDATA[");
        if (item_title_start != nullptr) {
            item_title_start += strlen("<title><![CDATA[");
            char* item_title_end = strstr(item_title_start, "]]></title>");
            if (item_title_end != nullptr) {
                size_t title_length = item_title_end - item_title_start;
                std::string news_title(item_title_start, title_length);

                switch (count) {
                    case 0:
                        News1 = news_title;
                        break;
                    case 1:
                        News2 = news_title;
                        break;
                    case 2:
                        News3 = news_title;
                        break;
                }

                ++count;
            }
        }

        item_start = strstr(item_start + strlen("<item>"), "<item>");
    }

    //printf("%s",response);
    printf("\nNews4: %s\n",News1.c_str());
    printf("News5: %s\n",News2.c_str());
    printf("News6: %s\n",News3.c_str());
    printf("Source2: %s",NewsSrc.c_str());
    socket.close();
}
void Get_News() {

    nsapi_size_or_error_t result;

    TCPSocket socket;

    result = socket.open(network);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to open socket: %d\n", result);
        return;
    }
   
    // Make a GET request to the API
    char request[512];
    snprintf(request, sizeof(request),
            "GET /news/rss.xml# HTTP/1.1\r\n"
            "Host: feeds.bbci.co.uk \r\n"
            "Connection: close\r\n"
            "\r\n");

    SocketAddress address;
    result = network->gethostbyname("feeds.bbci.co.uk", &address);


  // Checking the result
  if (result != 0) {
    printf("Failed to get IP address of host: %d\n", result);
    while (true);
  }
    
    address.set_port(80);
    result = socket.connect(address);
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to connect to server: %d\n", result);
        socket.close();
        return;
    }
    
    printf("Succeeded to connect to server\n");

    result = socket.send(request, strlen(request)); // Send the request
    if (result < 0) {
        printf("Failed to send request: %d\n", result);
        socket.close();
        return;
    }
    
    printf("Succeeded to send request\n");
    
    
    char response[4000];

    result = read_response(&socket, response, sizeof(response));// Receive the response
    if (result < 0) {
        printf("Failed to receive response: %d\n", result);
        socket.close();
        return;
    }

    printf("Succeeded to receive request\n");

    response[result] = '\0'; // Null-terminate the response string


    //printf("Response: %s\n",response);

    char *headline_begin = strchr(response, '<title><![CDATA[');
    char *headline_end = strchr(response, ']]></title>'); 

    // Check if we actually got any headline in the response
    if (headline_begin == nullptr || headline_end == nullptr) {
      printf("Failed to find headline in response\n");
      socket.close();
      return;
    }

    // Get the required information from the xml

    // This extracts the source the feed
    char* title_start = strstr(response, "<title><![CDATA[");
    char* title_end = nullptr;
    if (title_start != nullptr) {
        title_start += strlen("<title><![CDATA[");
        title_end = strstr(title_start, "]]></title>");
        if (title_end != nullptr) {
            size_t src_length = title_end - title_start;
            NewsSrc = std::string(title_start, src_length);
        }
    }

    // This extracts the top 3 news.
    int count = 0;
    char* item_start = strstr(response, "<item>");
    while (item_start != nullptr && count < 3) {
        char* item_title_start = strstr(item_start, "<title><![CDATA[");
        if (item_title_start != nullptr) {
            item_title_start += strlen("<title><![CDATA[");
            char* item_title_end = strstr(item_title_start, "]]></title>");
            if (item_title_end != nullptr) {
                size_t title_length = item_title_end - item_title_start;
                std::string news_title(item_title_start, title_length);

                switch (count) {
                    case 0:
                        News1 = news_title;
                        break;
                    case 1:
                        News2 = news_title;
                        break;
                    case 2:
                        News3 = news_title;
                        break;
                }

                ++count;
            }
        }

        item_start = strstr(item_start + strlen("<item>"), "<item>");
    }

    printf("\nNews1: %s\n",News1.c_str());
    printf("News2: %s\n",News2.c_str());
    printf("News3: %s\n",News3.c_str());
    printf("Source: %s",NewsSrc.c_str());
    socket.close();
}

void DisplayNews() {

    lcd.clear();
    Get_News();
    thread_sleep_for(1000);

    DisplayRunning = true;

    std::string newsStr = "                     " + News1 + " ||| " + News2 + " ||| " + News3;
    int newsLen = newsStr.length();

    // Calculate the number of characters needed to scroll
    int scrollLen = newsLen + 16;

    while (DisplayRunning) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("%s", NewsSrc.c_str());

        for (int i = scrollLen - 1; i >= 0 && DisplayRunning; i--) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.printf("%s", NewsSrc.c_str());

            int substrIndex = (scrollLen - i) % newsLen;

            // Extract the substring to display
            std::string displayStr = newsStr.substr(substrIndex, 16);

            // Calculate the starting column for displaying the substring
            int startColumn = 16 - displayStr.length();

            lcd.setCursor(startColumn, 1);
            lcd.printf("%s", displayStr.c_str());
            ThisThread::sleep_for(150ms);
        }
    }
}

void DisplayNews2() {
    lcd.clear();
    Get_News_2();
    thread_sleep_for(1000);
    DisplayRunning = true;

    std::string newsStr = "                     " + News1 + " ||| " + News2 + " ||| " + News3;
    int newsLen = newsStr.length();

    // Calculate the number of characters needed to scroll
    int scrollLen = newsLen + 16;

    while (DisplayRunning) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("  %s", NewsSrc.c_str());

        for (int i = scrollLen - 1; i >= 0 && DisplayRunning; i--) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.printf("  %s", NewsSrc.c_str());

            int substrIndex = (scrollLen - i) % newsLen;

            // Extract the substring to display
            std::string displayStr = newsStr.substr(substrIndex, 16);

            // Calculate the starting column for displaying the substring
            int startColumn = 16 - displayStr.length();

            lcd.setCursor(startColumn, 1);
            lcd.printf("%s", displayStr.c_str());
            ThisThread::sleep_for(150ms);
        }
    }
}

void DisplayNews3() {
    lcd.clear();
    Get_News_3();
    thread_sleep_for(1000);
    DisplayRunning = true;

    std::string newsStr = "                     " + News1 + " ||| " + News2 + " ||| " + News3;
    int newsLen = newsStr.length();

    // Calculate the number of characters needed to scroll
    int scrollLen = newsLen + 16;

    while (DisplayRunning) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("%s", NewsSrc.c_str());

        for (int i = scrollLen - 1; i >= 0 && DisplayRunning; i--) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.printf("%s", NewsSrc.c_str());

            int substrIndex = (scrollLen - i) % newsLen;

            // Extract the substring to display
            std::string displayStr = newsStr.substr(substrIndex, 16);

            // Calculate the starting column for displaying the substring
            int startColumn = 16 - displayStr.length();

            lcd.setCursor(startColumn, 1);
            lcd.printf("%s", displayStr.c_str());
            ThisThread::sleep_for(150ms);
        }
    }
}

void TimeSetup() {

    set_time(epochTime);  // Set the time to epochTime

    int Offset_seconds = 3600 * timezone_offset;  // Calculate seconds of the timezone offset

    if (DaylightSavingTime) {
        Offset_seconds += 3600;  // Add 1 hour if DaylightSavingTime is true
    }

    time_t currentTime = time(NULL);  // Get the current time in seconds
    time_t adjustedTime = currentTime + Offset_seconds;  // Calculate the adjusted time

    set_time(adjustedTime);  // Set the time to the adjusted time

}

void AlarmMute() {
    IsAlarmMuted = true;
    AlarmIsSounding = false;
    alarm_hours_Snoozed = alarm_hours;
    alarm_minutes_Snoozed = alarm_minutes;
}

void AlarmSnooze() {
    if (SnoozedCountPressed < SnoozedCount) {
        IsAlarmSnoozed = true;
        AlarmIsSounding = false;
        SnoozedCountPressed++;
        alarm_minutes_Snoozed +=  2;
        if (alarm_minutes_Snoozed >= 60) {
            alarm_hours_Snoozed += 1;
            alarm_minutes_Snoozed = alarm_minutes_Snoozed % 60;
        }
    }
}

void DisplayTime() {
    DisplayRunning = true;
    while (DisplayRunning) {
        time_t timeinfo = time(NULL);
        lcd.printf("%s", ctime(&timeinfo));
        if (IsAlarmEnabled) {
            lcd.setCursor(0, 1);
            lcd.printf("Alarm: %02i:%02i", alarm_hours, alarm_minutes); 
        }
        ThisThread::sleep_for(1000ms);
        lcd.clear();
    }
}

void AlarmHoursIncrease() {

    alarm_hours += 1; // Increases the alarm hours with 1

    if (alarm_hours >= 24) { // if max hours is reached it will wrap around
            alarm_hours = 0;
    }
}

void AlarmMinutesIncrease() {
    
    alarm_minutes += 1; // Increases the alarm minutes with 1

    if (alarm_minutes >= 60) { // if max minutes is reached it will wrap around
            alarm_minutes = 0;
    }
} 

void ChangeAlarmStaus() {
    IsAlarmEnabled = !IsAlarmEnabled;
}

void DisplayAlarmSetup() {

    DisplayRunning = true;
    printf("Snoozed: %02i:%02i\n",alarm_hours_Snoozed,alarm_minutes_Snoozed);
    while (DisplayRunning) {
        buttonAction1.fall(&AlarmMinutesIncrease);
        buttonAction2.fall(&AlarmHoursIncrease);
        buttonAction3.fall(&ChangeAlarmStaus);
        lcd.clear();
        lcd.printf("Set alarm:");
        lcd.setCursor(0, 1);
        lcd.printf("%02i:%02i ", alarm_hours,alarm_minutes); 
        ThisThread::sleep_for(1000ms);
        if (SnoozedCountPressed == 0) {
            alarm_hours_Snoozed = alarm_hours;
            alarm_minutes_Snoozed = alarm_minutes;
        }
        
        printf("Snoozed: %02i:%02i\n",alarm_hours_Snoozed,alarm_minutes_Snoozed);
    }
    
}

void DisplayTempPressure() {
    DisplayRunning = true;
    float temperature;
    float weather_pressure;
    sensor.enable();
    while (DisplayRunning) {
        sensor.get_temperature(&temperature);
        lcd.printf("Temp: %0.1f C", temperature);
        lcd.setCursor(0, 1);
        lcd.printf("Airp: %0.1f mb", weather_pressure); 
        ThisThread::sleep_for(1000ms);
        lcd.clear();
    }
}

// Array with the functions to the displays
void (*Screens[])() = {
    DisplayTime, 
    DisplayWeather,
    DisplayTempPressure,
    DisplayLatLon, 
    DisplayAlarmSetup, 
    DisplayNews,
    DisplayNews2,
    DisplayNews3
};

void AlarmThread() {
    while (true) {
        // Get the current time
        time_t now = time(NULL);
        struct tm* timeinfo = localtime(&now);

        // Check if the alarm is enabled and the current time matches the alarm time
        if (IsAlarmEnabled && timeinfo->tm_hour == alarm_hours_Snoozed && timeinfo->tm_min == alarm_minutes_Snoozed) {
            DisplayRunning = false;
            AlarmIsSounding = true;
            buttonAction1.fall(&AlarmMute);
            buttonAction2.fall(&AlarmSnooze);
            
            // Calculate the end time of the alarm duration (10 seconds from now)
            time_t now = time(NULL);
            time_t duration = now + 10;
            // Activate the alarm and wait for the duration
            while (now < duration && (!IsAlarmMuted || !IsAlarmSnoozed)) {
                if (IsAlarmMuted || IsAlarmSnoozed)  {
                    alarm.write(0);
                }
                else {
                    alarm.write(0.5);                    
                }
                now = time(NULL);
            }
            printf("test\n");
            alarm.write(0);
            AlarmIsSounding = false;
            IsAlarmMuted = false;
            IsAlarmSnoozed = false;
            if (SnoozedCountPressed >= (SnoozedCount + 1)) {
                alarm_hours_Snoozed = alarm_hours;
                alarm_minutes_Snoozed = alarm_minutes;
                SnoozedCountPressed = 0;
            }
            ThisThread::sleep_for(60s);
        }    

        ThisThread::sleep_for(1000ms);  // Adjust the sleep duration as needed
    }
}

// Function that handles the screen next
void NextScreen()
{
    if (!AlarmIsSounding) { // if the alarm is sounding it stops the user from switching screens
        ISNEXT = true;
        DisplayRunning = false;
    }
}

// Function that handles the screen prev
void PrevScreen()
{
    if (!AlarmIsSounding) { // if the alarm is sounding it stops the user from switching screens
        ISPREV = true;
        DisplayRunning = false;
    }
}


int main() {

    lcd.init();
    sensor.init(NULL);
    sensor.enable();

    connect_network();

    // Fetch and display the startup data
    UNIXDATA();  

    // Setup the time
    TimeSetup();

    NEXT.fall(&NextScreen);
    PREV.fall(&PrevScreen);

    // Sets the screen to the default screen
    (*Screens[ScreenIndex])();

     // Start the alarm thread
    alarmThread.start(AlarmThread);
    
    // Main program loop
    while (true) {
        led1 = !led1;
        while (AlarmIsSounding) {
            lcd.clear();
            lcd.printf("WAKE UP!");
            ThisThread::sleep_for(1s);
            if (!AlarmIsSounding) {
                lcd.clear();
                (*Screens[ScreenIndex])();
            }
        }
        if (ISNEXT) {  

            ISNEXT = false;

            // Increment the screen index
            ScreenIndex++;

            // Wrap around to the first screen if we reach the end
            if (ScreenIndex == (sizeof(Screens) / sizeof(Screens[0]))) {
                ScreenIndex = 0;
            }

            // Clear the LCD display
            lcd.clear();

            // Call the current function
            (*Screens[ScreenIndex])();
        }

        if (ISPREV) {

            ISPREV = false;

            // Decrement the screen index
            ScreenIndex--;

            // Wrap around to the last screen if we reach the beginning
            if (ScreenIndex < 0) {
                ScreenIndex = (sizeof(Screens) / sizeof(Screens[0])) - 1;
            }
            
            // Clear the LCD display
            lcd.clear();

            // Call the current function
            (*Screens[ScreenIndex])();
        }

    }

}