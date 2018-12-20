#pragma region Tray class
/* TRAY CLASS
Firmware for incubator tray operation. Defines Tray class object.

INITIALISATION: Call Init() function befor main loop. Pass screen object as an argument to the method.
* Initialisation of SD card and loggigng system -> create Log file for logging data and errorLog file for logging
operational reports.
* Initialise buttons.
* Load stored variables from preference class. Tray will store data on:
- flow_set - last set flow rate
- pos_syringe_1 - position of syringe 1
- pos_syringe_2 - position of syringe 2
- tray_name - tray name
- tray_number -  1 to 4
- Notification preferences for app: send_mail (send notification mail), send_push (send push notifications on
mobile app), system_notifications (send system notifications, i.e. operational errors), mail - stored mail.
* Initialise screen.
* Wifi setup with setupWiFi().
* Configure time from timeserver.
* Setup temperature sensors with setupSensors().

OPERATION:
* For operation switchState() method needs to be called each loop. Tray can be in various states.

* INNITIAL - initial state, enter only on reboot. Check if on last use, flow control was turned on -> go to OPERATION state. Else go to
STANDBY state.
* OPERATION - move syringes.
- get input from end swiches, change direction if needed.
- run motors and update speed.
* CHANGE_SYRINGE:
- get input from end swiches, change direction if needed.
- run motors and update speed.
- check for end position.

BUTTONS:
We have 4 buttons, from left to right: SET, OK, UP, DOWN. Its funcionality depends on the currnt tray state and screen.
* Check each loop for user input with getButtons().

LED SCREEN
* S_SETUP - upon reboot, it guides user trough wifi setup and running procedures
* S_OPERATION - main sreen in operation mode. Displays temperature and flow.
* S_SET_FLOW - Screen for seting flow value. Change value with UP and DOWN, when value is ok press OK. 
* S_CHANGE_SYRINGE - Screen during syringe change. It displays time left to finish. 
* S_MENU_OPERATION - first menu screen. If OK is pressed, flow control starts. Tray goes to OPERATION state.
* S_MENU_SET_FLOW - second menu screen. If OK is pressed, go to the set flow option. 
* S_MENU_CHANGE_SYRINGE - third menu screen. If OK is pressed, change to  syringes will go to pre-defined position.

*
Change of screens:
Main screens:
* S_SETUP -> (auto) S_OPERATION
		-> (auto) S_STANDBY
* S_OPERATION -> (B_SET) S_MENU_OPERATION (choose with B_OK or wait for 5s to get back to S_OPERATION)

* S_MENU_OPERATION -> S_MENU_SET_FLOW -> S_MENU_CHANGE_SYRINGE -> S_MENU_MANUAL -> S_MENU_STOP ->... (with B_SET button)

* S_MENU_OPERATION -> (B_OK) S_OPERATION: start tray flow control, go to trayState OPERATION if not there.
* S_MENU_SET_FLOW -> (B_OK) S_SET_FLOW: Sets value with UP and DOWN buttons. If OK is pressed new value is recorded, otherwise old one is kept.
* S_MENU_CHANGE_SYRINGE -> (B_OK) S_CHANGE_SYRINGE: Tray goes to CHANGE_SYRINGE state. Time to finish is displayed. 

*/
#pragma endregion


#ifndef Tray_h
#define Tray_h

#include <WiFi.h>     
#include <DNSServer.h>
#include <ESP32WebServer.h>
#include "WiFiManager.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_SHT31.h"
#include "FS.h"
#include "SD.h"
#include <time.h>
#include <stdarg.h>
#include <AccelStepper.h>

//Define error logging level
#define DEBUG 4
#pragma region Logging functions

//Alert
#if (DEBUG >= 0)
#define ALERT_PRINTLN(x){ Serial.println (x); errorfile.println (x);}
#define ALERT_PRINT(x){ Serial.print (x); errorfile.print (x);}
#else
#define ALERT_PRINTLN(x)
#define ALERT_PRINT(x)
#endif

//Critical
#if (DEBUG >= 1)
#define CRIT_PRINTLN(x){ Serial.println (x); errorfile.println (x);}
#define CRIT_PRINT(x){ Serial.print (x); errorfile.print (x);}
#else
#define CRIT_PRINTLN(x)
#define CRIT_PRINT(x)
#endif

//Error
#if (DEBUG >= 2)
#define ERR_PRINTLN(x){ Serial.println (x); errorfile.println (x);}
#define ERR_PRINT(x){ Serial.print (x); errorfile.print (x);}
#else
#define ERR_PRINTLN(x)
#define ERR_PRINT(x)
#endif

//Warning
#if (DEBUG >= 3)
#define WARN_PRINTLN(x){ Serial.println (x); errorfile.println (x);}
#define WARN_PRINT(x){ Serial.print (x); errorfile.print (x);}
#else
#define WARN_PRINTLN(x)
#define WARN_PRINT(x)
#endif

//Informational
#if (DEBUG >= 4)
#define INFO_PRINTLN(x){ Serial.println (x); errorfile.println (x);}
#define INFO_PRINT(x){ Serial.print (x); errorfile.print (x);}
#else
#define INFO_PRINTLN(x)
#define INFO_PRINT(x)
#endif

//Debug
#if (DEBUG >= 5)
#define DEBUG_PRINTLN(x){ Serial.println (x); errorfile.println (x);}
#define DEBUG_PRINT(x){ Serial.print (x); errorfile.print (x);}
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#endif

#pragma endregion

//INNITIAL settings
#define OLED_RESET -1
#define INNITIAL_T 37.0
#define INNITIAL_FLOW 0.1
#define INNITIAL_MANUAL_FLOW 20
#define FLOW_STEP 0.05
#define FLOW_MAX 20
#define INNITIAL_POS_SYRINGE_1 0 //Initial position syringe 1
#define INNITIAL_POS_SYRINGE_2 0 //Initial position syringe 1
#define INNITIAL_DIR_SYRINGE_1 -1 //Initial direction of movement syringe 1
#define INNITIAL_DIR_SYRINGE_2 1 //Initial direction of movement syringe 
#define STEPPER_MAX_SPEED 10000 //Max alowed speed
#define INNITIAL_TIME_CHANGE_SYRINGE 200 //Max time for syringe change in s

//LED screen parameters
#define TEXT_OFFSET 4 //Horizontal text offset from line
#define HEIGHT_SMALL 9 //Heighth of small text
#define HEIGHT_LARGE 20 //Height of large text
#define HEIGHT_IP 10 //Position of line below printed IP
#define HEIGHT_TEXT 14 //Start vertical position of text above main values
#define HEIGHT_TEMP 30 //Start vertical position of main values on main screen
#define HEIGHT_SET 27 //Start vertical position of main values on set screen
#define HEIGHT_BUTTONS 54 //Position of line above buttons 
#define VERTICAL_OFFSET 3 //Vertical text offset
#define HORIZONTAL_OFFSET 6 //Horizontal text offset
#define FLOW_OFFSET 10 //Offset of flow rate value
#define INACTIVE_SCREEN_TIME 10000 //Inactive screen time to change back to main

//Pins
#define BUTTON_1 34
#define BUTTON_2 36
#define BUTTON_3 39
#define BUTTON_4 4
#define STEPPER2_DIR_PIN 33
#define STEPPER2_STEP_PIN 32
#define STEPPER1_DIR_PIN 27
#define STEPPER1_STEP_PIN 14
#define ENABLE_PIN 12
#define END_1_STEPPER_1 A0
#define END_2_STEPPER_1 A1
#define END_1_STEPPER_2 17
#define END_2_STEPPER_2 21

enum ScreenState{  
	S_SETUP,
	S_OPERATION,
	S_SET_FLOW,
	S_CHANGE_SYRINGE,
	S_MANUAL,
	S_STANDBY,
	S_MENU_OPERATION,
	S_MENU_SET_FLOW,
	S_MENU_CHANGE_SYRINGE,
	S_MENU_MANUAL, 
	S_MENU_STOP, 
	S_MENU_SYRINGE1,
	S_MENU_SYRINGE2,
	S_MENU_SYRINGE_BOTH,
	S_MENU_BACK
};

enum TrayState {
	INITIAL,
	OPERATION,
	CHANGE_SYRINGE,
	MANUAL_MOVE,
	STANDBY,
	OFF,
	ALERT
};

enum ButtonState {
	B_UP,
	B_DOWN,
	B_SET,
	B_OK,
	B_NULL
};

class Errors {

public:
	Errors();
	void Init();

private:
};

class Tray{
public:
	
	Tray();
	
	void Init(Adafruit_SSD1306*);
	void switchState();

	//SD card logging
	File logfile;
	File errorfile;
	char filename[30];
	char error_filename[30];
	struct tm tmstruct;
	int log_time;

	void LOG(int level, const char *text, ...); //Log function

	//ERRORS
	Errors ERROR;

	//Get variables
	String get_tray_name();
	float get_T();
	float get_flow();
	float get_flow_set();
	float get_flow_manual();
	float get_pos_syringe(int);
	bool get_manual();
	bool get_regulation();
	bool get_change_syringe();
	const char* get_token();

	//Set variables
	void set_flow(float);
	void set_manual_flow(float);
	void set_manual(bool);
	void set_move_syringe(int);
	void set_flow_regulation(bool);
	void set_change_syringe(bool);
	void set_tray_name(String);

	//Settings
	void reset_WiFi();

	bool send_mail;
	bool send_push;
	bool flow_notifications;
	bool system_notifications;
	String mail;

	//WEB interface pages
	String getPage();
	String getSettings();
	String getReset();

	
private:

	TrayState trayState;
	ScreenState screenState;
	int screenStateTimer; 

	Preferences preferences;
	String tray_name = "TrayName";
	int tray_number;


	//BUTTONS
	ButtonState button_1;
	ButtonState button_2;
	ButtonState button_3;
	ButtonState button_4;

	ButtonState getButtons();
	void switchButtons();

	//TEMPERATURE
	
	//Temperature sensors inside incubator
	Adafruit_SHT31 temp_sensor;
	bool temp_sensor_connected; //temp sensor 1
	
	float T; //Temperature sensor 1

	//MOTORS
	AccelStepper* stepper1;
	AccelStepper* stepper2;

	//FLOW
	bool operation; //Operation flow regulation on/off
	int manual_syringe; //Manual syringe choosen 0-non 1-first, 2-second, 3-both
	float flow_set; //Set flow level
	float flow_set_new; //New set value
	float flow; //Current flow rate
	int speed; //Current flow rate motor speed
	
	//SYRINGE 1 and 2
	int dir_1; //Current direction syringe 1
	int dir_2; //Current direction syringe 2
	float pos_syringe_1; //Position syringe 1
	float pos_syringe_2; //Position syringe 2
	//MANUAL
	float flow_manual; //Manual mode flow rate
	int flow_manual_speed; //Manual mode flow motor speed
	int flow_dir; //Movement direction
	int time_change_syringe; //Time left for change syringe cycle

	void getEndSwitch(); //Get output from end switches
	void runMotors(); //Move motors
	int updateSwitchTime(); //Update time to finish syringe change
	bool checkEndPosition(); //Check if final position was reached in syringe move mode
	
	//Go-To state methods
	void goToStandby(); //Go to standby state
	void goToOperation(); //Go to operation state
	void goToChangeSyringe(); //Go to change syring state
	void goToManualSyringe(int); //Go to manual mode

	//Methods
	void setupSensors(); //Initialise sensors
	void updateScreen(); //Change display values
	bool updateTemperature(); //Update temperature readings
	float updateValue(float, float, float); //Increase/decrease value
	void logData(); //Log data to SD card
	
	//WiFi
	WiFiManager *wifiManager;
	const char* localSSID = "Tray";
	const char* localPassword = "tray";
	IPAddress deviceIP;
	bool WiFi_connected;

	bool setupWiFi();

	//Oled
	Adafruit_SSD1306 *oled;

	//Blynk
	//Blynk token
	const char* TOKEN_TRAY_1 = "a8c6aaf3cf784a6490f1c01cad5a5375";
	const char* TOKEN_TRAY_2 = "f6766c116eda4576a70fc083a2868640";
	const char* TOKEN_TRAY_3 = "97eda50ba9644e4ebdeef5d5ca547bff";
	const char* TOKEN_TRAY_4 = "ef9253c178b14030baae76c3ae1e9110";
	
	//Print functions
	void printScreenInitial();
	void printScreenConnected();
	void printScreenSetConnection();
	void printScreenOffline();
	void printScreenStandby();
	void printScreenMenu(const char*, const char*);
	void printScreenOperation(float, float);
	void printScreenChange();
	void printScreenTime(int);
	void printScreenBackground();
	void printScreenIP();
	void printScreenButtons();
	void printScreenTemperature(float);
	void printScreenFlow(float);
	void printScreenSet();
	void printScreenSetValue(float, int);
	void printScreenManual();
};

#endif