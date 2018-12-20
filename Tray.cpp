#include "Tray.h"

//ERRORS
Errors::Errors() {}

void Errors::Init() {

}

//TRAY

Tray::Tray() {

	trayState = TrayState::INITIAL;
	screenState = ScreenState::S_SETUP;
	//flow_regulation = true;
	send_mail = true;
	send_push = true;
	flow_notifications = true;
	system_notifications = true;
	mail = "";

	T = INNITIAL_T;
	flow = 0.0;
	flow_set = INNITIAL_FLOW;
	flow_set_new = INNITIAL_FLOW;
	pos_syringe_1 = INNITIAL_POS_SYRINGE_1;
	pos_syringe_2 = INNITIAL_POS_SYRINGE_2;

	//Stepers
	stepper1 = new AccelStepper(AccelStepper::DRIVER, STEPPER1_STEP_PIN, STEPPER1_DIR_PIN);
	stepper2 = new AccelStepper(AccelStepper::DRIVER, STEPPER2_STEP_PIN, STEPPER2_DIR_PIN);

	//Wifi manager
	wifiManager = new WiFiManager();
}

#pragma region void Tray::Init(Adafruit_SSD1306* Oled)
/*Initialise Tray class
Input:
	Adafruit_SSD1306* Oled - LED screen instance
Output:
Description:
* Initialise log files and SD card.
* Define buttons.
* Initialise variables.
* Load stored values from preferences.
* Initialise LED screen
* Initialise WiFi connection.
* Initialise time.
* Sensors setup.
*/
void Tray::Init(Adafruit_SSD1306* Oled){
	
	//Initalise error states
	ERROR.Init();

	sprintf(error_filename, "/errorLog.txt");
	sprintf(filename, "/Log.txt");

	//Start SD card and log file
	if (!SD.begin()) {
		LOG(2, "SD card Mount Failed!");
	}
	logfile = SD.open(filename);
	errorfile = SD.open(error_filename);
	if (!logfile) {
		LOG(2, "Log file does not exist, create new one.");
		logfile = SD.open(filename, FILE_WRITE);
		if (!logfile) {
			LOG(2, "Couldnt create data log file.");
		}
		if (logfile.print("TIME,")) {
			logfile.print("TEMP_S1,");
			logfile.print("TEMP_S2,");
			logfile.print("TEMP_HEATER,");
			logfile.print("TEMP_SET,");
			logfile.print("CO2_S1,");
			logfile.println("CO2_SET");
			LOG(4, "Data log file written.");
		}
		else {
			LOG(2, "Data log file write failed!");
		}
	}
	if (!errorfile) {
		LOG(2, "Log file does not exist, create new one.");
		errorfile = SD.open(error_filename, FILE_WRITE);
		if (!errorfile) {
			LOG(2, "Couldnt create error log file.");
		}
		if (errorfile.println("Start error log.")) {
			LOG(4, "Error log file written.");
		}
		else {
			LOG(2, "Error log file write failed!");
		}
	}

	logfile.close();
	errorfile.close();
	errorfile = SD.open(error_filename, FILE_APPEND);
	LOG(2, "Error file opened for appending!");
	
	//buttons
	button_1 = ButtonState::B_SET;
	pinMode(BUTTON_1, INPUT);
	button_2 = ButtonState::B_OK;
	pinMode(BUTTON_2, INPUT);
	button_3 = ButtonState::B_UP;
	pinMode(BUTTON_3, INPUT);
	button_4 = ButtonState::B_DOWN;
	pinMode(BUTTON_4, INPUT);

	LOG(4, "Buttons initialised!");
	
	//variables
	trayState = TrayState::INITIAL;
	screenState = ScreenState::S_SETUP;
	temp_sensor_connected = false;

	//Saved values
	//Start preferences
	preferences.begin("incubator", false);
	flow_set = preferences.getFloat("setFlow", INNITIAL_FLOW);
	flow_manual = preferences.getFloat("manualFlow", INNITIAL_MANUAL_FLOW);
	pos_syringe_1 = preferences.getFloat("pos1", INNITIAL_POS_SYRINGE_1);
	pos_syringe_2 = preferences.getFloat("pos2", INNITIAL_POS_SYRINGE_2);
	dir_1 = preferences.getFloat("dir1", INNITIAL_DIR_SYRINGE_1);
	dir_2 = preferences.getFloat("dir2", INNITIAL_DIR_SYRINGE_2);
	operation = preferences.getBool("operation", false);
	tray_name = preferences.getString("trayName", "TrayName");
	tray_number = preferences.getInt("trayNumber", 1);
	send_mail = preferences.getBool("sendMail", true);
	send_push = preferences.getBool("sendPush", true);
	system_notifications = preferences.getBool("sendSystem", true);
	mail = preferences.getString("mail", "");
	preferences.end();

	//Flow and steppers
	pinMode(ENABLE_PIN, OUTPUT);
	digitalWrite(ENABLE_PIN, LOW);

	pinMode(END_1_STEPPER_1, INPUT);
	pinMode(END_2_STEPPER_1, INPUT);
	pinMode(END_1_STEPPER_2, INPUT);
	pinMode(END_2_STEPPER_2, INPUT);

	manual_syringe = 0; //None in manual syringe is chosen
	time_change_syringe = INNITIAL_TIME_CHANGE_SYRINGE;
	flow_set_new = flow_set;
	stepper1->setMaxSpeed(STEPPER_MAX_SPEED);
	stepper2->setMaxSpeed(STEPPER_MAX_SPEED);
	stepper1->setSpeed(0);
	stepper2->setSpeed(0);


	//setup screen
	oled = Oled;
	oled->begin(SSD1306_SWITCHCAPVCC, 0x3c);  // initialize with the I2C addr 0x3D (for the 128x64)
	printScreenInitial();

	LOG(4, "Screen initialised!");
	LOG(4, "Saved settings:");
	LOG(4, "Tray name: %s", tray_name.c_str());
	LOG(4, "Mail notifications: %d", send_mail);
	LOG(4, "Mail: %s", mail.c_str());
	LOG(4, "Push notifications: %d", send_push);

	//setup WiFi
	WiFi_connected = setupWiFi();

	//Setup clock and logfile
	configTime(0, 0, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
	delay(2000);
	tmstruct.tm_year = 0;
	getLocalTime(&tmstruct, 5000);
	sprintf(filename, "/%d-%02d-%02d-%02d-%02d-%02d.txt", (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
	if (tmstruct.tm_year) {
		LOG(4, "Time synchronised: %d-%02d-%02d %02d:%02d:%02d", (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
	}
	else {
		LOG(2, "Time synchronisation failed!");
	}
	
	//setup temperature sensors and make initial reading
	setupSensors();
}
#pragma endregion

#pragma region void Tray::switchState()
/* Switch between different tray states and adjust response
Input: /
Output: /
Description:
* INNITIAL: 
	- Display screen.
	- go to OPERATION or MANUAL_MOVE
* OPERATION: normal tray operation - regulate flow rate, move motors, collect clicked buttons, log data.
	- Switch buttons.
	- Update sensor readings
	- Regulate flow.
* STANDBY
* OFF
* ALERT
*/
void Tray::switchState() {

	switch (trayState)
	{
	case TrayState::INITIAL:
	{
		//Setup incubator
		LOG(5, "STATE - Innitial");
		
		if (operation)
		{
			goToOperation();
		}
		else
		{
			goToStandby();
		}
		screenStateTimer = millis();
		break;
	}
	case TrayState::OPERATION:
	{
		LOG(5, "STATE - Operation");
		getEndSwitch(); //Get end switch data
		break;
	}
	case TrayState::CHANGE_SYRINGE:
	{
		LOG(5, "STATE - Change syringe.");
		getEndSwitch(); //Get end switch data
		//Check if action compleated
		if (checkEndPosition()) {
			goToStandby();
		}

		break;
	}
	case TrayState::MANUAL_MOVE:
	{
		LOG(5, "STATE - Manual.");
		Serial.print(dir_1);
		Serial.print(" ");
		Serial.println(dir_2);
		getEndSwitch();
		break;
	}
	case TrayState::STANDBY:
	{
		LOG(5, "STATE - Standby");
		break;
	}
	case TrayState::OFF:
	{
		LOG(5, "STATE - Off");
		break;
	}
	case TrayState::ALERT:
	{
		LOG(0, "STATE - ALERT!");
		LOG(0, "Closing errorlog file!");
		LOG(0, "Restarting ESP!");
		errorfile.close();
		break;
	}
	}

	//Get buttons input and change screen
	switchButtons();
	updateScreen();
	runMotors(); //Move

	//Write to SD card
	if (millis() - log_time > 10000)
	{
		logData();
		log_time = millis();
	}
}
#pragma endregion

#pragma region void Tray::switchButtons()
/* Respond to pressed buttons
Input: /
Output: /
Description: 
	* B_MAIN - change to main screen.
	* B_SET_FLOW - change ro set flow screen.
	* B_SET_FLOW_MANUAL - change to set manual flow screen.
	* B_MANUAL - change to manual flow control. 
	* B_UP_1 - 
	* B_DOWN_1 - 
	* B_UP_2 -
	* B_DOWN_2 - 
*/
void Tray::switchButtons()
{
	//Get buttons
	switch (getButtons())
	{
	case ButtonState::B_SET:
	{
		//Change menu screens
		if (screenState == ScreenState::S_MENU_OPERATION) {

			screenState = ScreenState::S_MENU_SET_FLOW;
			printScreenMenu("Set flow", "");
		}
		else if (screenState == ScreenState::S_MENU_SET_FLOW) {

			screenState = ScreenState::S_MENU_CHANGE_SYRINGE;
			printScreenMenu("Change", "syringe");
		}
		else if (screenState == ScreenState::S_MENU_CHANGE_SYRINGE) {

			screenState = ScreenState::S_MENU_MANUAL;
			printScreenMenu("Manual", "mode");
		}
		else if (screenState == ScreenState::S_MENU_MANUAL) {

			screenState = ScreenState::S_MENU_STOP;
			printScreenMenu("Stop", "");
		}
		else if (screenState == ScreenState::S_MENU_STOP) {
			
			screenState = ScreenState::S_MENU_OPERATION;
			printScreenMenu("Operation", "");
		}
		else if (screenState == ScreenState::S_MENU_SYRINGE1) {

			screenState = ScreenState::S_MENU_SYRINGE2;
			printScreenMenu("Syringe 2", "");
		}
		else if (screenState == ScreenState::S_MENU_SYRINGE2) {

			screenState = ScreenState::S_MENU_SYRINGE_BOTH;
			printScreenMenu("Syringe", "both");
		}
		else if (screenState == ScreenState::S_MENU_SYRINGE_BOTH) {

			screenState = ScreenState::S_MENU_BACK;
			printScreenMenu("Exit", "");
		}
		else if (screenState == ScreenState::S_MENU_BACK) {

			screenState = ScreenState::S_MENU_SYRINGE1;
			printScreenMenu("Syringe 1", "");
		}
		else
		{
			if (trayState == TrayState::MANUAL_MOVE)
			{
				screenState = ScreenState::S_MENU_SYRINGE1;
				printScreenMenu("Syringe 1", "");
			}
			else
			{
				screenState = ScreenState::S_MENU_OPERATION;
				printScreenMenu("Operation", "");
			}
		}
		screenStateTimer = millis();
		break;
	}
	case ButtonState::B_OK:
	{
		if (screenState == ScreenState::S_MENU_OPERATION) {
			goToOperation(); //Start operation
		}
		else if (screenState == ScreenState::S_MENU_SET_FLOW) {
			printScreenSet();
		}
		else if (screenState == ScreenState::S_MENU_CHANGE_SYRINGE) {
			goToChangeSyringe();
		}
		else if (screenState == ScreenState::S_MENU_MANUAL) {

			goToManualSyringe(3); //Go to manual mode
		}
		else if (screenState == ScreenState::S_MENU_STOP) {
			goToStandby(); //Stop the system and go to standby
		}
		else if (screenState == ScreenState::S_MENU_SYRINGE1) {

			goToManualSyringe(1);
		}
		else if (screenState == ScreenState::S_MENU_SYRINGE2) {

			goToManualSyringe(2);
		}
		else if (screenState == ScreenState::S_MENU_SYRINGE_BOTH) {

			goToManualSyringe(3); //Go to manual mode
		}
		else if (screenState == ScreenState::S_MENU_BACK) {
			goToStandby(); //Stop the system and go to standby
		}
		else if (screenState == ScreenState::S_SET_FLOW) {
			if (flow_set_new != flow_set)
			{
				flow_set = flow_set_new;
				preferences.begin("incubator", false);
				preferences.putFloat("setFlow", flow_set);
				preferences.end();

				//If flow control, update speed
				if (operation) {
					flow = flow_set;
					stepper1->setSpeed(dir_1 * speed);
					stepper2->setSpeed(dir_2 * speed);
				}
			}
			screenState = ScreenState::S_MENU_OPERATION;
			printScreenMenu("Operation", "");
		}
		break;
	}
	case ButtonState::B_UP:
	{
		if (screenState == ScreenState::S_SET_FLOW) {
			LOG(5, "Button UP pressed. Increase set flow.");
			flow_set_new = updateValue(flow_set_new, FLOW_STEP, 1.0);
			if (flow_set_new > FLOW_MAX)
			{
				flow_set_new = FLOW_MAX;
			}
			printScreenSetValue(flow_set_new, 2);
			screenStateTimer = millis();
		}
		else if (screenState == ScreenState::S_MANUAL) {
			LOG(5, "Button UP pressed. Move syringe 1.");
			switch (manual_syringe) {
			case 1:
			{
				if (dir_1 == -1) {
					dir_1 = 0;
				}
				else {
					dir_1 = 1;
				}
				dir_2 = 0;
				break;
			}
			case 2:
			{
				dir_1 = 0;
				if (dir_2 == -1) {
					dir_2 = 0;
				}
				else {
					dir_2 = 1;
				}
				break;
			}
			case 3:
			{
				if (dir_1 == -1) {
					dir_1 = 0;
					dir_2 = 0;
				}
				else {
					dir_1 = 1;
					dir_2 = -1;
				}
				break;
			}
			}
			stepper1->setSpeed(dir_1*speed);
			stepper2->setSpeed(dir_2*speed);

			screenStateTimer = millis();
		}
		delay(200);
		break;
	}
	case ButtonState::B_DOWN:
	{
		if (screenState == ScreenState::S_SET_FLOW) {
			LOG(5, "Button DOWN pressed. Decrease set flow.");
			flow_set_new = updateValue(flow_set_new, FLOW_STEP, -1.0);
			if (flow_set_new < 0.0)
			{
				flow_set_new = 0.0;
			}
			printScreenSetValue(flow_set_new, 2);
			screenStateTimer = millis();
		}
		else if (screenState == ScreenState::S_MANUAL) {
			LOG(5, "Button DOWN pressed. Move syringe 1.");
			switch (manual_syringe) {
			case 1:
			{
				if (dir_1 == 1) {
					dir_1 = 0;
				}
				else {
					dir_1 = -1;
				}
				dir_2 = 0;
				break;
			}
			case 2:
			{
				dir_1 = 0;
				if (dir_2 == 1) {
					dir_2 = 0;
				}
				else {
					dir_2 = -1;
				}
				break;
			}
			case 3:
			{
				if (dir_1 == 1) {
					dir_1 = 0;
					dir_2 = 0;
				}
				else {
					dir_1 = -1;
					dir_2 = 1;
				}
				break;
			}
			}
			stepper1->setSpeed(dir_1*speed);
			stepper2->setSpeed(dir_2*speed);
			screenStateTimer = millis();
		}
		delay(200);
		break;
	}
	}
}
#pragma endregion

#pragma region ButtonState Tray::getButtons()
/*Check if any button is pressed
Input: /
Output: ButtonState - state which was called
Description:
* Collect button input and evoke new state. 
*/
ButtonState Tray::getButtons() {
	
	ButtonState tmpState = ButtonState::B_NULL;
	
	//Check if any button is pressed
	int tmp = digitalRead(BUTTON_1);
	if (tmp) {
		LOG(5, "Pressed nr. 1 button.");
		tmpState = button_1;
	}

	tmp = digitalRead(BUTTON_2);
	if (tmp) {
		LOG(5, "Pressed nr. 2 button.");
		tmpState = button_2;
	}

	tmp = digitalRead(BUTTON_3);
	if (tmp) {
		LOG(5, "Pressed nr. 3 button.");
		tmpState = button_3;
	}

	tmp = digitalRead(BUTTON_4);
	if (tmp) {
		LOG(5, "Pressed nr. 4 button.");
		tmpState = button_4;
	}

	return tmpState;
}
#pragma endregion

#pragma region bool Tray::setupWiFi()
/* Setup WiFi connection
Input: /
Output: bool connected - true/false if connected. 
Description:
* set up WiFi manager.
* if possible connect to wifi, otherwise guide user trough wifi setup. 
*/
bool Tray::setupWiFi(){

	LOG(4, "Begin WiFi setup.");
	bool connected = false;
	//WiFiManager wifiManager;
	wifiManager->setDebugOutput(false);
	//reset settings - for testing
	//wifiManager->resetSettings();
	//set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
	//wifiManager.setAPCallback(configModeCallback);

	int autoCon = wifiManager->autoConnect(localSSID, localPassword);
	if (autoCon == 0) {
		deviceIP = WiFi.softAPIP();
		if (wifiManager->printOffline()) {
			LOG(4, "Begin offline operation.");
			printScreenOffline();
		}
		else {
			LOG(3, "Failed to connect and hit timeout. Reseting...");
			//reset and try again, or maybe put it to deep sleep
			ESP.restart();
			delay(1000);
		}
	}
	else if (autoCon == 1) {
		//if you get here you have connected to the WiFi
		LOG(4, "Connected to WiFi!");
		deviceIP = WiFi.localIP();
		printScreenConnected();
		connected = true;
	}
	else {
		// Enter setup mode
		LOG(4, "Enter connection setup mode.");
		printScreenSetConnection();
		if (!wifiManager->startConfigPortal(localSSID, localPassword)) {
			deviceIP = WiFi.softAPIP();
			if (wifiManager->printOffline()) {
				printScreenOffline();
			}
			else {
				LOG(3, "Failed to connect and hit timeout. Reseting...");
				//reset and try again, or maybe put it to deep sleep
				ESP.restart();
				delay(1000);
			}
		}
		else {
			//if you get here you have connected to the WiFi
			LOG(4, "Connected to WiFi!");
			deviceIP = WiFi.localIP();
			printScreenConnected();
			connected = true;
		}
		LOG(4, "Restarting ESP!");
		ESP.restart();
		delay(1000);
	}
	LOG(4, "Connected to: %s", WiFi.SSID().c_str());
	LOG(4, "Device IP: %s", deviceIP.toString().c_str());

	return connected;
}
#pragma endregion

void Tray::reset_WiFi(){

	LOG(4, "Reseting WiFi settings and ESP!");
	//reset settings - for testing
	wifiManager->resetSettings();
	ESP.restart();
	delay(1000);
}

void Tray::updateScreen()
{
	switch (screenState)
	{
	case ScreenState::S_MENU_OPERATION:
	case ScreenState::S_MENU_SET_FLOW:
	case ScreenState::S_MENU_CHANGE_SYRINGE:
	case ScreenState::S_MENU_MANUAL:
	case ScreenState::S_MENU_STOP:
	case ScreenState::S_MENU_SYRINGE1:
	case ScreenState::S_MENU_SYRINGE2:
	case ScreenState::S_MENU_SYRINGE_BOTH:
	case ScreenState::S_MENU_BACK:
	case ScreenState::S_SET_FLOW:
	{
		//Check if non-active screen form more than 10s
		if (millis() - screenStateTimer > INACTIVE_SCREEN_TIME)
		{
			if (trayState == TrayState::OPERATION) {
				//Start operation
				printScreenOperation(T, flow);
			}
			else if (trayState == TrayState::CHANGE_SYRINGE) {
				printScreenChange();
			}
			else if (trayState == TrayState::MANUAL_MOVE) {
				printScreenManual();
			}
			else if (trayState == TrayState::STANDBY) {
				printScreenStandby();
			}
			screenStateTimer = millis();
		}
		return;
	}
	case ScreenState::S_OPERATION:
	{
		return;
	}
	case ScreenState::S_CHANGE_SYRINGE:
	{
		int tmp_time = updateSwitchTime();
		if (time_change_syringe - tmp_time > 1)
		{
			time_change_syringe = tmp_time;
			printScreenTime(time_change_syringe);
		}
	}
	}
}

void Tray::runMotors() {

	if (dir_1*speed != 0 || dir_2*speed != 0) {
		digitalWrite(ENABLE_PIN, HIGH);
		pos_syringe_1 = stepper1->currentPosition();
		pos_syringe_2 = stepper2->currentPosition();
		stepper1->runSpeed();
		stepper2->runSpeed();
	}
	else
	{
		digitalWrite(ENABLE_PIN, LOW);
	}
}

void Tray::getEndSwitch() {
	if (digitalRead(END_1_STEPPER_1))
	{
		stepper1->setCurrentPosition(0);
		if (operation) {
			dir_1 = -1;
			stepper1->setSpeed(dir_1 * speed);

		}
		else {
			if (dir_1 == 1) {
				dir_1 = 0;
				stepper1->setSpeed(dir_1 * speed);
				stepper1->runSpeed();
			}
		}
		LOG(4, "Reached end switch 1.");
	}
	if (digitalRead(END_2_STEPPER_1))
	{
		if (operation) {
			dir_1 = 1;
			stepper1->setSpeed(dir_1 * speed);
		}
		else {
			if (dir_1 == -1) {
				dir_1 = 0;
				stepper1->setSpeed(dir_1 * speed);
				stepper1->runSpeed();
			}
		}
		Serial.println(stepper1->currentPosition());
		LOG(4, "Reached end switch 2.");
	}
	if (digitalRead(END_1_STEPPER_2))
	{
		stepper2->setCurrentPosition(0);
		if (operation) {
			dir_2 = -1;
			stepper2->setSpeed(dir_2 * speed);
		}
		else {
			if (dir_2 == 1) {
				dir_2 = 0;
				stepper2->setSpeed(dir_2 * speed);
				stepper2->runSpeed();
			}
		}
		stepper2->setSpeed(dir_2 * speed);
		LOG(4, "Reached end switch 3.");
	}
	if (digitalRead(END_2_STEPPER_2))
	{
		if (operation) {
			dir_2 = 1;
			stepper2->setSpeed(dir_2 * speed);
		}
		else {
			if (dir_2 == -1) {
				dir_2 = 0;
				stepper2->setSpeed(dir_2 * speed);
				stepper2->runSpeed();
			}
		}
		Serial.println(stepper2->currentPosition());
		LOG(4, "Reached end switch 4.");
	}
}

void Tray::goToChangeSyringe() {
	
	operation = true; //Stop flow control and store value
	preferences.begin("incubator", false);
	preferences.putBool("operation", operation);
	preferences.end();

	trayState = TrayState::CHANGE_SYRINGE; //Update Tray state
	time_change_syringe = updateSwitchTime(); //Update end time

	//Change current flow and speed
	//...
	flow = flow_set;
	speed = 500;
	stepper1->setSpeed(dir_1 * speed);
	stepper2->setSpeed(dir_2 * speed);

	printScreenChange();
}

void Tray::goToStandby() {
	operation = false; //Stop flow control and store value
	preferences.begin("incubator", false);
	preferences.putBool("operation", operation);
	preferences.end();

	//Set curent flow and speed to 0
	flow = 0.0;
	speed = 0;

	stepper1->setSpeed(dir_1 * speed);
	stepper2->setSpeed(dir_2 * speed);

	trayState = TrayState::STANDBY; //Change to standby state
	printScreenStandby(); //Print standby screen 
}

void Tray::goToOperation() {

	operation = true; //Stop flow control and store value
	preferences.begin("incubator", false);
	preferences.putBool("operation", operation);
	preferences.end();
	trayState = TrayState::OPERATION;

	flow = flow_set;
	speed = 500;
	stepper1->setSpeed(dir_1 * speed);
	stepper2->setSpeed(dir_2 * speed);

	printScreenOperation(T, flow);
}

void Tray::goToManualSyringe(int syringe) {

	operation = false; //Stop flow control and store value
	preferences.begin("incubator", false);
	preferences.putBool("operation", operation);
	preferences.end();

	//Set flow and 
	flow = flow_manual;
	speed = 1000;

	dir_1 = 0;
	dir_2 = 0;
	stepper1->setSpeed(dir_1 * speed);
	stepper2->setSpeed(dir_2 * speed);

	manual_syringe = syringe; //Choose manual syringe
	printScreenManual();

	trayState = TrayState::MANUAL_MOVE;
}

int Tray::updateSwitchTime() {
	//Update switch time based on the position and speed....

	return 200;
}

bool Tray::checkEndPosition() {

	if (pos_syringe_1 == 0 && pos_syringe_2 == 0) {
		//return true;
		return false;
	}
	else {
		return false;
	}
}

#pragma region void Tray::setupSensors()
/* Setup temperature and co2 sensors
Input: / 
Output: /
Description:
	* Setup one temperature sensor. If connected set temp_sensor_connected_1 to true.
	* Make temperature readings. 
*/
void Tray::setupSensors(){

	//Sensor temperature
	temp_sensor = Adafruit_SHT31();
	if (!temp_sensor.begin(0x45)) {   // Set to 0x45 for alternate i2c addr
		LOG(1, "Couldn't find SHT31 sensor!");
		while (1) delay(1);
	}
	else
	{
		temp_sensor_connected = true;
		float t = temp_sensor.readTemperature();
		LOG(4, "SHT31 sensor setup done. First reading: %.2f C.", t);
	}
	updateTemperature();
	delay(2000);
}
#pragma endregion

#pragma region bool Tray::updateTemperature()
/* Update sensor temperature readings
Input: /
Output: bool change - true if average temperature on the tray has changed.
Description:
	* Make reading from temperature sensor, provided it is connected. 
	* Determine if there is change.
*/
bool Tray::updateTemperature() {
	
	//Sensor 1
	float tmp = T;
	bool change = false;
	if (temp_sensor_connected)
	{
		//Make new reading 
		tmp = temp_sensor.readTemperature();
		LOG(5, "Temperature sensor: %.2f C.", T);
	}
	
	if (abs((round(T * 10.0) / 10.0) - (round(tmp * 10.0) / 10.0)) >= 0.1)
	{
		change = true;
		T = tmp;
	}

	return change;
}
#pragma endregion

#pragma region float Tray::updateValue(float val, float step, float sgn)
/* Update value for desired step
Input:
	* float val - original value
	* float step - desired increment
	* float sgn - desired sign of increment (+/- 1)
Output:
	* float - updated value
*/
float Tray::updateValue(float val, float step, float sgn)
{
	return val + sgn * step; 
}
#pragma endregion

void Tray::logData()
{
	//Update time
	getLocalTime(&tmstruct, 5000);
	//Open file
	logfile = SD.open(filename, FILE_APPEND);
	//PRINT DATA
	logfile.printf("%d-%02d-%02d %02d:%02d:%02d,", (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);

	logfile.close();
}

//PRINT FUNCTIONS

#pragma region void Tray::printScreenInitial()
/* Print innitial screen
Input: /
Output: / 
Description:
	* Print innitial welcome screen.
*/
void Tray::printScreenInitial(){
  oled->clearDisplay();
  oled->setTextSize(2);
  oled->setTextColor(WHITE);
  oled->setCursor(0,0);
  oled->println("Welcome!");
  oled->setTextSize(1);
  oled->setCursor(0,20);
  oled->println("The setup state will begin shortly.");
  oled->println("Wait for the instructions...");
  oled->display();
  delay(2000);
}
#pragma endregion

#pragma region void Tray::printScreenSetConnection()
/* Print set connection screen
Input: /
Output: /
Description:
	* Print instructions to connect to locala incubator WiFi with set password. 
*/
void Tray::printScreenSetConnection() {
  oled->clearDisplay();
  oled->setTextSize(1);
  oled->setTextColor(WHITE);
  oled->setCursor(0,0);
  oled->print("Connect to local WiFi: ");
  oled->setCursor(0,12);
  oled->setTextSize(2);
  oled->println(localSSID);
  oled->setCursor(0,31);
  oled->setTextSize(1);
  oled->print("WiFi password: ");
  oled->setCursor(0,43);
  oled->setTextSize(2);
  oled->println(localPassword);
  //oled->print("Local IP address: "); 
  //oled->println(WiFi.softAPIP());
  oled->display();
  delay(1);
}
#pragma endregion

#pragma region void Tray::printScreenConnected() 
/* Print connection details screen
Input: /
Output: /
Description:
* Print connection detail, once device is connected to wifi.
*/
void Tray::printScreenConnected(){
  oled->clearDisplay();
  oled->setTextSize(1);
  oled->setTextColor(WHITE);
  oled->setCursor(0,0);
  oled->println("Connected to WiFi:");
  oled->setCursor(0,12);
  oled->setTextSize(2);
  oled->println(WiFi.SSID());
  oled->setCursor(0,31);
  oled->setTextSize(1);
  oled->println("Device IP address:");
  oled->setCursor(0,45);
  oled->println(WiFi.localIP());
  oled->display();
  delay(2000);
}
#pragma endregion

#pragma region void Tray::printScreenOffline()
/* Print offline mode screen
Input: /
Output: /
Description:
* Print offline mode, if connection cannot be stablished or user choosed this option.
*/
void Tray::printScreenOffline(){
  oled->clearDisplay();
  oled->setTextSize(1);
  oled->setTextColor(WHITE);
  oled->setCursor(0,0);
  oled->println("Starting offline operation mode.");
  oled->display();
  delay(1);
}
#pragma endregion

void Tray::printScreenBackground()
{
	//Background
	oled->clearDisplay();
	oled->drawLine(0, HEIGHT_IP, oled->width() - 1, HEIGHT_IP, WHITE);
	oled->drawLine(0, HEIGHT_BUTTONS, oled->width() - 1, HEIGHT_BUTTONS, WHITE);

	//IP
	printScreenIP();
	//Buttons
	printScreenButtons();
}

void Tray::printScreenStandby() {
	//screen state
	screenState = ScreenState::S_STANDBY;

	//Background
	printScreenBackground();
	oled->setTextSize(2);
	oled->setCursor(0, HEIGHT_TEMP);
	oled->println("Standby");
	oled->display();
	delay(1);
}

void Tray::printScreenMenu(const char* title1, const char* title2) {
	//Background
	printScreenBackground();
	oled->setTextSize(2);
	oled->setCursor(0, HEIGHT_TEXT);
	oled->println(title1);
	oled->print(title2);
	oled->display();
	delay(1);
}

#pragma region void Tray::printScreenOperation(float t, float fl)
/* Print main screen
Input: 
	* IPAddress ip - tray ip address
	* float t - tray temperature
	* float fl - tray flow rate
Output: /
Description:
	* Setup the main screen.
	* Print ip, button values, temperature and flow levels. 
*/
void Tray::printScreenOperation(float t, float fl){
	
	DEBUG_PRINTLN("Setting up the main sreen.");
	//screen state
	screenState = ScreenState::S_OPERATION;

	//Background
	printScreenBackground();

	oled->drawLine(oled->width() / 2, HEIGHT_IP, oled->width() / 2, HEIGHT_BUTTONS, WHITE);
	oled->setTextSize(1);
	oled->setCursor(0,HEIGHT_TEXT);
	oled->println("TEMP(C)");
	oled->setCursor(oled->width()/2 + TEXT_OFFSET,HEIGHT_TEXT);
	oled->println("FLOW(ml/m)");
	
	//Temperature
	printScreenTemperature(t);
	//CO2
	printScreenFlow(fl);
	oled->display();
	delay(1);
}
#pragma endregion

void Tray::printScreenChange()
{
	DEBUG_PRINTLN("Setting up the main sreen.");
	//screen state
	screenState = ScreenState::S_CHANGE_SYRINGE;

	//Background
	printScreenBackground();
	oled->setTextSize(1);
	oled->setCursor(0, HEIGHT_TEXT);
	oled->println("Left time");
	printScreenTime(time_change_syringe);
	delay(1);
}

void Tray::printScreenTime(int sec) {
	//Convert sec to min:sec
	int min = (int)(sec / 60);
	sec -= min * 60;
	//Clear previous reading
	oled->fillRect(0, HEIGHT_TEMP, oled->width(), HEIGHT_LARGE, BLACK);
	//Write new
	oled->display();
	oled->setTextSize(2);
	oled->setCursor(0, HEIGHT_TEMP);
	if (min < 10)
	{
		oled->print(0);
	}
	oled->print(min);
	oled->print(":");
	oled->print(sec);
	oled->display();
}

#pragma region void Tray::printScreenSet()
/* Print setup screen for temperature and co2 value
Input:
	* IPAddress ip - incubator ip address
Output: /
Description:
	* Setup the background screen.
	* Print ip, button values.
	* Based on the screen state either display desired temperature or co2 value. 
*/
void Tray::printScreenSet() {

	screenState = ScreenState::S_SET_FLOW;
	LOG(5, "Setting up the set screen.");
	//Background
	printScreenBackground();
	oled->setCursor(0, HEIGHT_TEXT);
	oled->setTextSize(1);
	oled->println("SET FLOW (mL/min)");

	if (flow_set < 10.0)
	{
		printScreenSetValue(flow_set, 2);
	}
	else if (flow_set < 100.0)
	{
		printScreenSetValue(flow_set, 1);
	}
	else
	{
		printScreenSetValue(flow_set, 0);
	}
	
	oled->display();
	delay(1);
}
#pragma endregion

#pragma region void Tray::printScreenIP()
/* Print IP address of the tray
Input: IPAddress
Output: /
Description:
	* Print IP address at the top of the screen. 
*/
void Tray::printScreenIP(){
  //Clear previous ip
  oled->fillRect(0, 0, oled->width()-1, HEIGHT_SMALL, BLACK);
  //Re-write
  oled->setTextSize(1);
  oled->setCursor(0,0);
  oled->print("IP: ");
  oled->println(deviceIP);
  oled->display();
}
#pragma endregion

#pragma region void Tray::printScreenButtons()
/* Print value of the left button
Input: /
Output: /
Description: /
*/
void Tray::printScreenButtons(){
  oled->setTextSize(1);
  oled->setCursor(0, HEIGHT_BUTTONS + VERTICAL_OFFSET);
  oled->println("SET");
  oled->drawLine(oled->width() / 4, HEIGHT_BUTTONS, oled->width() / 4, oled->height(), WHITE);
  oled->setCursor(oled->width() / 4 + 2*HORIZONTAL_OFFSET, HEIGHT_BUTTONS + VERTICAL_OFFSET);
  oled->println("OK");
  oled->drawLine(oled->width() / 2, HEIGHT_BUTTONS, oled->width() / 2, oled->height(), WHITE);
  oled->setCursor(oled->width() / 2 + 2*HORIZONTAL_OFFSET, HEIGHT_BUTTONS + VERTICAL_OFFSET);
  oled->write(0x18);
  oled->drawLine(3 * oled->width() / 4, HEIGHT_BUTTONS, 3 * oled->width() / 4, oled->height(), WHITE);
  oled->setCursor(3 * oled->width() / 4 + 2*HORIZONTAL_OFFSET, HEIGHT_BUTTONS + VERTICAL_OFFSET);
  oled->write(0x19);
  oled->display();
  delay(1);
}
#pragma endregion

#pragma region void Tray::printScreenTemperature(float temp)
/* Print temperature on the main screen
Input: float temp - temperature
Output: /
Description:
	* Print temperature, rounded to one decimal place on the left side of the main screen.
*/
void Tray::printScreenTemperature(float temp){
  //Clear previous reading
  oled->fillRect(0,HEIGHT_TEMP, oled->width()/2-1 - FLOW_OFFSET, HEIGHT_LARGE, BLACK);
  //Write new
  oled->display();
  oled->setTextSize(2);
  oled->setCursor(0,HEIGHT_TEMP);
  if (temp < 100.0)
  {
	  oled->print(temp, 1);
  }
  else
  {
	  oled->print(temp, 0);
  }
  oled->display();
  delay(1);
}
#pragma endregion

#pragma region void Tray::printScreenSetValue(float val, int precision)
/* Print set value screen
Input: 
	* float val - value of either temperature or co2
	* int precision - number of decimal places
Output: /
Description:
	* Print set value screen, with given value.
*/
void Tray::printScreenSetValue(float val, int precision) {
	DEBUG_PRINTLN("Setting value....");
	//Clear previous reading
	oled->fillRect(0, HEIGHT_SET, oled->width() / 2, HEIGHT_LARGE, BLACK);
	//Write new
	oled->setTextSize(2);
	oled->setCursor(0, HEIGHT_SET);
	oled->print(val, precision);
	oled->display();
	delay(1);
}
#pragma endregion

#pragma region void Tray::printScreenFlow(float fl)
/* Print flow rate on the main screen
Input: float fl - flow rate
Output: /
Description:
* Print flow, rounded to two or three decimal places on the right side of the main screen.
*/
void Tray::printScreenFlow(float fl){
  //Clear previous reading
  oled->fillRect(oled->width()/2+ HORIZONTAL_OFFSET, HEIGHT_TEMP, oled->width(), HEIGHT_LARGE, BLACK);
  //Write new
  oled->display();
  oled->setTextSize(2);
  oled->setCursor(oled->width()/2 + HORIZONTAL_OFFSET, HEIGHT_TEMP);
  if (fl < 10.0)
  {
	  oled->print(fl, 2);
  }
  else if (fl < 100.0)
  {
	  oled->print(fl, 1);
  }
  else
  {
	  oled->print(fl, 0);
  }
  oled->display();
  delay(1);
}
#pragma endregion

void Tray::printScreenManual()
{
	DEBUG_PRINTLN("Setting up the manual sreen.");
	//screen state
	screenState = ScreenState::S_MANUAL;

	//Background
	printScreenBackground();
	oled->setTextSize(1);
	oled->setCursor(0, HEIGHT_TEXT);
	oled->print("SYRINGE - ");
	if (manual_syringe == 1)
	{
		oled->print(1);
	}
	else if (manual_syringe == 2)
	{
		oled->print(2);
	}
	else if (manual_syringe == 3)
	{
		oled->print("BOTH");
	}
	else
	{
		oled->print("NONE");
	}
	oled->print(" (mL/m)");

	printScreenFlow(flow);
	//Temperature
	oled->display();
	delay(1);
}

// GET FUNCTIONS
String Tray::get_tray_name() {
	return tray_name;
};

float Tray::get_flow_set() {
	return flow_set;
};

float Tray::get_flow() {
	return flow;
};

float Tray::get_flow_manual() {
	return flow_manual;
};

float Tray::get_pos_syringe(int num)
{
	if (num == 1)
	{
		return pos_syringe_1;
	}
	else
	{
		return pos_syringe_2;
	}
}

bool Tray::get_manual() {
	if (trayState == TrayState::MANUAL_MOVE) {
		return true;
	}
	else {
		return false;
	}
}

bool Tray::get_regulation() {
	if (trayState == TrayState::OPERATION) {
		return true;
	}
	else {
		return false;
	}
}

bool Tray::get_change_syringe() {
	if (trayState == TrayState::CHANGE_SYRINGE) {
		return true;
	}
	else {
		return false;
	}
}

float Tray::get_T() {
	return T;
};

const char* Tray::get_token() {
	switch (tray_number) {
	case 1: return(TOKEN_TRAY_1);
	case 2: return(TOKEN_TRAY_2);
	case 3: return(TOKEN_TRAY_3);
	case 4: return(TOKEN_TRAY_4);
	}
}

// SET functions

void Tray::set_flow(float tmpF)
{
	flow_set_new = tmpF;
	if (flow_set_new > FLOW_MAX)
	{
		flow_set_new = FLOW_MAX;
	}
	if (flow_set_new < 0.0)
	{
		flow_set_new = 0.0;
	}
	if (flow_set_new != flow_set) {
		flow_set = flow_set_new;
		preferences.begin("incubator", false);
		preferences.putFloat("setFlow", flow_set);
		preferences.end();
		screenState = ScreenState::S_SET_FLOW;
	}
		
	printScreenSet();
	screenStateTimer = millis();

	//If flow control, update speed
	if (operation) {
		flow = flow_set;
		stepper1->setSpeed(dir_1 * speed);
		stepper2->setSpeed(dir_2 * speed);
	}
}

void Tray::set_manual_flow(float tmpF)
{
	flow_manual = tmpF;
	if (flow_manual > FLOW_MAX)
	{
		flow_manual = FLOW_MAX;
	}
	if (flow_manual < 0.0)
	{
		flow_manual = 0.0;
	}
	preferences.begin("incubator", false);
	preferences.putFloat("manualFlow", flow_manual);
	preferences.end();

	if (trayState == TrayState::MANUAL_MOVE) {
		flow = flow_manual;
		stepper1->setSpeed(dir_1 * speed);
		stepper2->setSpeed(dir_2 * speed);
		printScreenManual();
	}
}

void Tray::set_manual(bool tmp)
{
	if (tmp)
	{
		goToManualSyringe(3);
		preferences.begin("incubator", false);
		preferences.putBool("operation", false);
		preferences.end();
	}
	else
	{
		goToStandby();
	}

	screenStateTimer = millis();
}

void Tray::set_flow_regulation(bool tmp)
{
	if (tmp)
	{
		goToOperation();
	}
	else
	{
		goToStandby();
	}

	preferences.begin("incubator", false);
	preferences.putBool("operation", tmp);
	preferences.end();
	screenStateTimer = millis();
}

void Tray::set_change_syringe(bool tmp)
{
	if (tmp)
	{
		
		Serial.println("change");
		goToChangeSyringe();
	}
	else
	{
		goToStandby();
	}

	preferences.begin("incubator", false);
	preferences.putBool("operation", tmp);
	preferences.end();
	screenStateTimer = millis();
}

void Tray::set_move_syringe(int mode)
{
	/*if (manual && !flow_regulation)
	{
		switch (mode)
		{
		case 0:
		{
			return;
		}
		case 1:
		{
			return;
		}
		case 2:
		{
			return;
		}
		case 3:
		{
			return;
		}
		case 4:
		{
			return;
		}
		case 5:
		{
			return;
		}
		}
	}*/
}

void Tray::set_tray_name(String name)
{
	tray_name = name;
	preferences.begin("incubator", false);
	preferences.putString("trayName", tray_name);
	preferences.end();
}

//WEB INTERFACE

String Tray::getPage() {
	String page = "<!doctype html>";
	page += "<html lang='en'>";
	page += "  <head>";
	page += "    <!-- Required meta tags -->";
	page += "    <meta charset='utf-8'>";
	page += "    <meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'> ";
	page += "    <!-- Bootstrap CSS -->";
	page += "    <link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/css/bootstrap.min.css' integrity='sha384-WskhaSGFgHYWDcbwN70/dfYBj47jz9qbsMId/iRN3ewGhXQFZCSftd1LZCfmhktB' crossorigin='anonymous'> ";
	page += "  <!-- jQuery -->";
	page += "  <script src='https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js'></script>";
	page += "  <!-- JavaScript -->";
	page += "  <script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js' integrity='sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa' crossorigin='anonymous'></script>";
	page += "    <title>Test WiFi interface</title>";
	page += "  <script>";
	page += "    function GetSensorValues()";
	page += "    {";
	page += "      var request = new XMLHttpRequest();";
	page += "      request.onreadystatechange = function(){";
	page += "        if(this.readyState == 4 && this.status == 200){";
	page += "          if(this.responseText != null){";
	page += "            var a = JSON.parse(this.responseText);";
	page += "            document.getElementById('flow').innerHTML = a.flow;";
	page += "            document.getElementById('flow_set').innerHTML = a.flow_set;";
	page += "            document.getElementById('manual_set').innerHTML = a.flow_manual;";
	page += "            document.getElementById('t').innerHTML = a.t;";
	page += "    		 var elem = document.getElementById('regulateButton');";
	page += "    		 if (a.regulate == 0) elem.value = 'REGULATE';";
	page += "    			else elem.value = 'STOP';";
	page += "    		 var elem1 = document.getElementById('changeButton');";
	page += "    		 if (a.change == 0) elem1.value = 'CHANGE SYRINGE';";
	page += "    			else elem1.value = 'STOP CHANGE';";
	page += "    		 var elem2 = document.getElementById('startButton');";
	page += "    		 if (a.manual == 0) elem2.value = 'START';";
	page += "    			else elem2.value = 'STOP';}";
	page += "        }";
	page += "      }; ";
	page += "      request.open('GET', 'getValues', true);";
	page += "      request.send();";
	page += "      setTimeout('GetSensorValues()', 10000);";
	page += "    }";
	page += "    function SetFlow()";
	page += "    {";
	page += "      var request = new XMLHttpRequest();";
	page += "      var f = document.getElementById('flow_new').value;";
	page += "      document.getElementById('flow_new').value = '';";
	page += "      request.open('GET', 'setFlow?flow='+f, true);";
	page += "      request.send();";
	page += "    }";
	page += "    function SetManual()";
	page += "    {";
	page += "      var request = new XMLHttpRequest();";
	page += "      var f = document.getElementById('manual_new').value;";
	page += "      document.getElementById('manual_new').value = '';";
	page += "      request.open('GET', 'setFlowManual?mflow='+f, true);";
	page += "      request.send();";
	page += "    }";
	page += "    function getSettings()";
	page += "    {";
	page += "      var request = new XMLHttpRequest();";
	page += "     request.onreadystatechange = function(){";
	page += "       if(this.readyState == 4 && this.status == 200){";
	page += "         document.getElementsByTagName('body')[0].innerHTML = this.responseText;";
	page += "       };";
	page += "     }; ";
	page += "      request.open('GET', 'settings', true);";
	page += "      request.send();";
	page += "    }";
	page += "    function settingsReturn()";
	page += "    {";
	page += "      var request = new XMLHttpRequest();";
	page += "     request.onreadystatechange = function(){";
	page += "       if(this.readyState == 4 && this.status == 200){";
	page += "         document.getElementsByTagName('body')[0].innerHTML = this.responseText;";
	page += "       };";
	page += "     }; ";
	page += "      request.open('GET', 'return', false);";
	page += "      request.send();";
	page += "    }";
	page += "    function resetWiFi()";
	page += "    {";
	page += "      var request = new XMLHttpRequest();";
	page += "     request.onreadystatechange = function(){";
	page += "       if(this.readyState == 4 && this.status == 200){";
	page += "         document.getElementsByTagName('body')[0].innerHTML = this.responseText;";
	page += "       };";
	page += "     }; ";
	page += "      request.open('GET', 'resetWiFi', false);";
	page += "      request.send();";
	page += "    }";
	page += "    function SaveSettings()";
	page += "    {";
	page += "      var request = new XMLHttpRequest();";
	page += "     request.onreadystatechange = function(){";
	page += "       if(this.readyState == 4 && this.status == 200){";
	page += "         document.getElementsByTagName('body')[0].innerHTML = this.responseText;";
	page += "       };";
	page += "     }; ";
	page += "      var name = document.getElementById('name').value;";
	page += "      document.getElementById('name').value = '';";
	page += "      request.open('GET', 'save?name='+name, true);";
	page += "      request.send();";
	page += "    }";
	page += "    function manualButton()";
	page += "    {";
	page += "       var request = new XMLHttpRequest();";
	page += "    	var elem = document.getElementById('startButton');";
	page += "    	var val = elem.value;";
	page += "    	if (elem.value == 'START') elem.value = 'STOP';";
	page += "    		else{ elem.value = 'START';";
	page += "    			var elem2 = document.getElementById('regulateButton');";
	page += "    			elem2.value = 'REGULATE';}";
	page += "       request.open('GET', 'startManual?status='+val, true);";
	page += "       request.send();";
	page += "    	}";
	page += "    function regulationButton()";
	page += "    {";
	page += "       var request = new XMLHttpRequest();";
	page += "    	var elem = document.getElementById('regulateButton');";
	page += "    	var val = elem.value;";
	page += "    	if (elem.value == 'STOP') elem.value = 'REGULATE';";
	page += "    		else {elem.value = 'STOP';";
	page += "    			var elem2 = document.getElementById('startButton');";
	page += "    			elem2.value = 'START';}";
	page += "       request.open('GET', 'startRegulation?status='+val, true);";
	page += "       request.send();";
	page += "    	}";
	page += "    function changingButton()";
	page += "    {";
	page += "       var request = new XMLHttpRequest();";
	page += "    	var elem = document.getElementById('changeButton');";
	page += "    	var val = elem.value;";
	page += "    	if (elem.value == 'STOP') elem.value = 'CHANGE SYRINGE';";
	page += "    		else {elem.value = 'STOP CHANGE';";
	page += "    			var elem2 = document.getElementById('regulateButton');";
	page += "    			elem2.value = 'REGULATE';}";
	page += "       request.open('GET', 'startChange?status='+val, true);";
	page += "       request.send();";
	page += "    	}";
	page += "    function moveSyringe(v)";
	page += "    {";
	page += "       var request = new XMLHttpRequest();";
	page += "       request.open('GET', 'moveSyringe?move='+v, true);";
	page += "       request.send();";
	page += "    	}";
	page += "  </script>";
	page += "  </head>";
	page += "  <body onload='GetSensorValues()' style='width: 80%; margin-right: auto; margin-left: auto; margin-top: 1%'>";
	page += "  <div>";
	page += "    <div class='row' style='background-color:LightGray;'>";
	page += "      <div class='col-md-8' style='padding-top: 2%; padding-bottom: 1%;'>";
	page += "        <h1>TRAY ";
	page += tray_number;
	page += "        : ";
	page += tray_name;
	page += "        </h1>";
	page += "      </div>";
	page += "      <div class='col-md-4'>";
	page += "        <div class='row'>";
	page += "          <div class='col-md-8' style='padding-top: 3%; padding-bottom: 2%;'>";
	page += "            <h3>Wifi: ";
	page += WiFi.SSID();
	page += "            </h3>";
	page += "            <h3>Tray IP: ";
	page += WiFi.localIP().toString();
	page += "            </h3>";
	page += "          </div>";
	page += "          <div class='col-md-4' style='padding-top: 1%; padding-bottom: 1%;'>";
	page += "            <button type='button' onclick='getSettings()' class='btn btn-primary btn-lg btn-block'>Settings</button>";
	page += "            <button type='button' onclick='resetWiFi()' class='btn btn-danger btn-lg btn-block'>Reset Wifi</button>";
	page += "          </div>";
	page += "        </div>";
	page += "      </div>";
	page += "    </div>";
	page += "    <div class='row' style='margin-top: 1%'>";
	page += "      <div class='col-md-6' style='margin: 0; padding: 0;'>";
	page += "      <div style='background-color:rgb(240, 240, 240); width: 99%; margin-right: 1%; padding: 2%;'>";
	page += "        <div class='row'>";
	page += "          <div class='col-md-6'>";
	page += "            <h2>FLOW RATE</h2>";
	page += "          </div>";
	page += "          <div class='col-md-3'>";
	page += "            <input type='button' id='changeButton' onclick='changingButton()' class='btn btn-primary btn-lg btn-block' value='STOP'/>";
	page += "          </div>";
	page += "          <div class='col-md-3'>";
	page += "            <input type='button' id='regulateButton' onclick='regulationButton()' class='btn btn-primary btn-lg btn-block' value='STOP'/>";
	page += "          </div>";
	page += "        </div>";
	page += "        <div class='row'>";
	page += "          <div class='col-md-4'>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <p>Current flow rate:</p>";
	page += "              </div>";
	page += "            </div>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <h2><span id='flow'>";
	page += flow;
	page += "                </span> mL/min</h2>";
	page += "              </div>";
	page += "            </div>";
	page += "          </div>";
	page += "          <div class='col-md-4'>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <p>Set flow rate:</p>";
	page += "              </div>";
	page += "            </div>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <h2><span id='flow_set'>";
	page += flow_set;
	page += "                </span> mL/min</h2>";
	page += "              </div>";
	page += "            </div>";
	page += "          </div>";
	page += "          <div class='col-md-4'>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <p>New flow rate:</p>";
	page += "              </div>";
	page += "            </div>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12' style='padding-top: 2%'>";
	page += "                <input type='text' id='flow_new' maxlength='4' size='4'>";
	page += "                <button type='button' onclick='SetFlow()' class='btn btn-success'>Submit</button>";
	page += "              </div>";
	page += "            </div>";
	page += "          </div>";
	page += "        </div>";
	page += "        <div class='row'>";
	page += "          <div class='col-md-12'>";
	page += "            <h3>Sensor readings:</h3>";
	page += "          </div>";
	page += "        </div>";
	page += "        <div class='row'>";
	page += "          <div class='col-md-4'>";
	page += "            <h4>Temperature: <b><span id='t'>";
	page += T;
	page += "            </span> &#8451;</b></h4>";
	page += "          </div>";
	page += "        </div>";
	page += "      </div>";
	page += "      </div>";
	page += "      <div class='col-md-6' style='margin: 0; padding: 0;'>";
	page += "      <div style='background-color:rgb(240, 240, 240); width: 99%; margin-left: 1%; padding: 2%;'>";
	page += "        <div class='row'>";
	page += "          <div class='col-md-9'>";
	page += "            <h2>MANUAL MOVE</h2>";
	page += "          </div>";
	page += "          <div class='col-md-3'>";
	page += "            <input type='button' id='startButton' onclick='manualButton()' class='btn btn-primary btn-lg btn-block' value='START'/>";
	page += "          </div>";
	page += "        </div>";
	page += "        <div class='row'>";
	page += "          <div class='col-md-1'>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <p>S 1:</p>";
	page += "              </div>";
	page += "            </div>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "				<div class='row' style='margin: 2%';>";
	page += "					<button type='button' onclick='moveSyringe(0)' class='btn btn-success'>&#9650;</button>";
	page += "              </div>";
	page += "				<div class='row' style='margin: 2%'>";
	page += "					<button type='button' onclick='moveSyringe(1)' class='btn btn-success'>&#9632;</button>";
	page += "              </div>";
	page += "				<div class='row' style='margin: 2%'>";
	page += "					<button type='button' onclick='moveSyringe(2)' class='btn btn-success'>&#9660;</button>";
	page += "              </div>";
	page += "              </div>";
	page += "            </div>";
	page += "          </div>";
	page += "          <div class='col-md-1'>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <p>S 2:</p>";
	page += "              </div>";
	page += "            </div>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "				<div class='row' style='margin: 2%';>";
	page += "					<button type='button' onclick='moveSyringe(3)' class='btn btn-success'>&#9650;</button>";
	page += "              </div>";
	page += "				<div class='row' style='margin: 2%'>";
	page += "					<button type='button' onclick='moveSyringe(4)' class='btn btn-success'>&#9632;</button>";
	page += "              </div>";
	page += "				<div class='row' style='margin: 2%'>";
	page += "					<button type='button' onclick='moveSyringe(5)' class='btn btn-success'>&#9660;</button>";
	page += "              </div>";
	page += "              </div>";
	page += "            </div>";
	page += "          </div>";
	page += "          <div class='col-md-1'>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <p>BOTH:</p>";
	page += "              </div>";
	page += "            </div>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "				<div class='row' style='margin: 2%';>";
	page += "					<button type='button' onclick='moveSyringe(6)' class='btn btn-success'>&#9650;</button>";
	page += "              </div>";
	page += "				<div class='row' style='margin: 2%'>";
	page += "					<button type='button' onclick='moveSyringe(7)' class='btn btn-success'>&#9632;</button>";
	page += "              </div>";
	page += "				<div class='row' style='margin: 2%'>";
	page += "					<button type='button' onclick='moveSyringe(8)' class='btn btn-success'>&#9660;</button>";
	page += "              </div>";
	page += "              </div>";
	page += "            </div>";
	page += "          </div>";
	page += "          <div class='col-md-4'>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <p>Set manual flow:</p>";
	page += "              </div>";
	page += "            </div>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <h2><span id='manual_set'>";
	page += flow_manual;
	page += "                </span> mL/min</h2>";
	page += "              </div>";
	page += "            </div>";
	page += "          </div>";
	page += "          <div class='col-md-4'>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12'>";
	page += "                <p>New manual flow:</p>";
	page += "              </div>";
	page += "            </div>";
	page += "            <div class='row'>";
	page += "              <div class='col-md-12' style='padding-top: 2%'>";
	page += "                <input type='text' id='manual_new' maxlength='4' size='4'>";
	page += "                <button type='button' onclick='SetManual()' class='btn btn-success'>Submit</button>";
	page += "              </div>";
	page += "            </div>";
	page += "          </div>";
	page += "        </div>";
	page += "      </div>";
	page += "      </div>";
	page += "    </div>";
	page += "  </div>";
	page += "  </body>";
	page += "</html>";

	return page;
}

String Tray::getSettings() {
	String page = "<body style='width: 80%; margin-right: auto; margin-left: auto; margin-top: 1%'>";
	page += " <div>";
	page += "   <div class='row' style='background-color:LightGray;'>";
	page += "      <div class='col-md-8' style='padding-top: 2%; padding-bottom: 1%;'>";
	page += "        <h1>TRAY: ";
	page += tray_name;
	page += "        </h1>";
	page += "      </div>";
	page += "     <div class='col-md-4'>";
	page += "       <div class='row'>";
	page += "         <div class='col-md-8' style='padding-top: 3%; padding-bottom: 2%;'>";
	page += "            <h3>Wifi: ";
	page += WiFi.SSID();
	page += "            </h3>";
	page += "            <h3>Tray IP: ";
	page += WiFi.localIP().toString();
	page += "            </h3>";
	page += "         </div>";
	page += "         <div class='col-md-4' style='padding-top: 1%; padding-bottom: 1%;'>";
	page += "           <button type='button' onclick='settingsReturn()' class='btn btn-primary btn-lg btn-block'>Back</button>";
	page += "           <button type='button' onclick='resetWiFi()' class='btn btn-danger btn-lg btn-block'>Reset Wifi</button>";
	page += "         </div>";
	page += "       </div>";
	page += "     </div>";
	page += "   </div>";
	page += "   <div class='row' style='margin-top: 1%; background-color:rgb(240, 240, 240);'>";
	page += "     <div class='col-md-12' >";
	page += "       <form>";
	page += "         <div class='form-group'>";
	page += "           <label for='usr'>Tray name:</label>";
	page += "           <input type='text' class='form-control' id='name'>";
	page += "         </div>";
	page += "         <div class='form-group'>";
	page += "           <label for='pwd'>Other setting</label>";
	page += "           <input type='text' class='form-control' id='pwd'>";
	page += "         </div>";
	page += "         </form>";
	page += "     </div>";
	page += "   </div>";
	page += "   <div class='row justify-content-end' style=' background-color:rgb(240, 240, 240);'>";
	page += "     <div class='col-md-2' style='padding-bottom: 1%;'>";
	page += "       <button type='button' onclick='SaveSettings()' class='btn btn-success btn-lg btn-block'>Save</button>";
	page += "     </div>";
	page += "   </div>";
	page += " </div>";
	page += "  </body>";
	return page;
}

String Tray::getReset() {
	String page = "<body style='width: 80%; margin-right: auto; margin-left: auto; margin-top: 1%'>";
	page += "  <div>";
	page += "    <p>Reseating WiFi... Please follow the instructions on the screen.</p>";
	page += "  </div>";
	page += "</body>";

	return page;
}

#pragma region void Tray::LOG(int level, const char* text, ...)
/* Logging function
Input:
* text - text string to be written
* int level - log level 0 - 5
* bool start - start new line of the log
* bool end - end line of the log
Output: /
Description:
* Specifie level at the start of the new line.
*/
void Tray::LOG(int level, const char* text, ...)
{
	//Combine text
	char msg[100];
	va_list  args;
	va_start(args, text);
	vsprintf(msg, text, args);
	va_end(args);

	//Write level specifier at the strat of the new line
	switch (level) {
	case 0:
	{
		ALERT_PRINT(millis());
		ALERT_PRINT("  ALERT: ");
		ALERT_PRINTLN(msg);
		return;
	}
	case 1:
	{
		CRIT_PRINT(millis());
		CRIT_PRINT("  CRITICAL: ");
		CRIT_PRINTLN(msg);
		return;
	}
	case 2:
	{
		ERR_PRINT(millis());
		ERR_PRINT("  ERROR: ");
		ERR_PRINTLN(msg);
		return;
	}
	case 3:
	{
		WARN_PRINT(millis());
		WARN_PRINT("  WARNING: ");
		WARN_PRINTLN(msg);
		return;
	}
	case 4:
	{
		INFO_PRINT(millis());
		INFO_PRINT("  INFO: ");
		INFO_PRINTLN(msg);
		return;
	}
	case 5:
	{
		DEBUG_PRINT(millis());
		DEBUG_PRINT("  DEBUG: ");
		DEBUG_PRINTLN(msg);
		return;
	}
	}
}
#pragma endregion