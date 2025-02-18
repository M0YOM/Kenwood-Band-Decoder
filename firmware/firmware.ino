/* 
  M0YOM Kenwood ESP32 Band Data Interface

  By James Thresher M0YOM
  https://www.m0yom.co.uk
  
  Tested with: TS-990S

  Reads the Serial data from the radio and outputs the current transmitting band in BCD format as used by Yaesu.

  The following Band Plan is used for determining the Band, this is based on the UK Band Plan

  Frequency (Mhz)       Band
  ---------------------------
  1.810-2000            160m
  3.500-3800            80m
  7.000-7200            40m
  14.000-14350          20m
  21.000-21450          15m
  28.000-29700          10m

  The BCD output matches the Yaesu standard

  D C B A   Band
  ---------------
  0 0 0 1   160m
  0 0 1 0   80m
  0 0 1 1   40m
  0 1 0 0   30m
  0 1 0 1   20m
  0 1 1 0   17m
  0 1 1 1   15m
  1 0 0 0   12m
  1 0 0 1   10m
  1 0 1 0   6m

  change log:
  Version 1.0 | 16 Feb 2025
              - Initial Version, based on UK Band Plan
  Version 1.1 | 18 Feb 2025
              - Added Tracking of last band output to avoid unneccesary updates to output pins
*/

// Enable Debug Mode, uncomment to enable debug statements
//#define DEBUG

// Pin Definitions
#define SERIAL2_RXD 16 // RX Pin to Kenwood COM Port
#define SERIAL2_TXD 17 // TX Pin to Kenwood COM Port
#define SERIAL2_BAUD 9600 // Kenwood COM Port speed
#define LED_BUILTIN 2 // Build in LED, used for debug and status
#define BCDOutput_A 32 // BCD Output A Pin
#define BCDOutput_B 33 // BCD Output B Pin
#define BCDOutput_C 25 // BCD Output C Pin
#define BCDOutput_D 26 // BCD Output D Pin

static int TXVFO = 0; // Our Transmitting VFO, 0 = VFO A, 1 = VFO B
static long VFOACurrentFreq = 0; // Our current VFO A frequency in hz
static long VFOBCurrentFreq = 0; // Our current VFO B frequency in hz
static long CurrentFreq = 0; // Our current transmit frequency in hz
static int CurrentBand = 0; // The current band we're outputting

static bool ConnectPollingActive = false; // Are we polling to reconnect
static bool PolledForData = false; // Have we actively polled for data, resets to false if data is received
static unsigned long LastConnectionPoll = 0; // The time since the last connection attempt was made
static unsigned long LastCommandReceived = 0; // The time since the last command was received
static unsigned long LastPolledForData = 0; // The time we polled for data
static const unsigned long POLLED_FOR_DATA_TIMEOUT = 2000; // ms How long without receiving a command before we attempt a reconnect
static const unsigned long LAST_COMMAND_TIMEOUT = 10000; // ms How long without receiving a command before we force an update
static const unsigned long LAST_CONNECT_POLL_TIMEOUT = 1000; // ms How often to attempt to reconnect

#ifdef DEBUG
static unsigned long FlashLED = 0; // Used for the flashing LED Debug Status
#endif

void setup() {

  // Setup our Pins
  pinMode(LED_BUILTIN, OUTPUT);  
  pinMode(BCDOutput_A, OUTPUT);
  pinMode(BCDOutput_B, OUTPUT);
  pinMode(BCDOutput_C, OUTPUT);
  pinMode(BCDOutput_D, OUTPUT);
  
  ResetSerialAndOutputs();

  // Setup our debug output
  #ifdef DEBUG
    Serial.begin(115200);    
  #endif

  ConnectToTS990S();
}

void ConnectToTS990S()
{
  // Open the serial port for the radio
  Serial2.begin(SERIAL2_BAUD, SERIAL_8N1, SERIAL2_RXD, SERIAL2_TXD);
  PollTS990SForData();

  #ifdef DEBUG
    Serial.println("Connection Started");
  #endif
}

void PollTS990SForData()
{  
  if (Serial2)
  {
    // put your setup code here, to run once:
    Serial2.print("AI2;"); // Turn on Auto Information output so we don't need to poll for updates
    delay(100); 
    Serial2.print("TB;"); // Request which VFO We're transmitting on
    delay(100);  
    Serial2.print("FA;"); // Request current VFO Frequencies
    delay(100);
    Serial2.print("FB;"); // Request current VFO Frequencies      
  }

  LastPolledForData = millis();
  PolledForData = true;
}

void ResetSerialAndOutputs()
{

  Serial2.end(); // Close the serial Port

  TXVFO = 0;
  VFOACurrentFreq = 0;
  VFOBCurrentFreq = 0;
  CurrentFreq = 0;
  CurrentBand = 0;

  // Set all pins to low to ensure no output until we know what to output
  digitalWrite(BCDOutput_A, LOW);  
  digitalWrite(BCDOutput_B, LOW);  
  digitalWrite(BCDOutput_C, LOW); 
  digitalWrite(BCDOutput_D, LOW); 
  
}


void loop() { 

  #ifdef DEBUG
    // Show that we're alive by flashing the on board LED once per second
    if (millis() - FlashLED >= 1000)
    {
      FlashLED = millis();
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  #endif

  // Reconnect
  if (millis() - LastCommandReceived >= LAST_COMMAND_TIMEOUT) {
    // We haven't had anything from the radio for a little while so poll it
    #ifdef DEBUG
      Serial.println("Last Command timed out, polling manually for data");
    #endif
    PollTS990SForData();
  }

  // in the event that we become disconnected from the TS990 but the Band Decoder remains active, attempt reconnect
  if (PolledForData == true && ConnectPollingActive == false)
  {
    if(millis() - LastPolledForData >= POLLED_FOR_DATA_TIMEOUT)
    {
        #ifdef DEBUG
          Serial.println("Data Poll timed out, starting reconnect polling");
        #endif

      ConnectPollingActive = true; // Nothing received after timeout so begin polling for reconnect
      LastConnectionPoll = millis();
      ResetSerialAndOutputs();
    }
  }

  // Are we polling for reconnection?
  if (ConnectPollingActive)
  {
    if(millis() - LastConnectionPoll >= LAST_CONNECT_POLL_TIMEOUT)
    {
        #ifdef DEBUG
          Serial.println("Attempting Reconnect");
        #endif

      LastConnectionPoll = millis();      
      ConnectToTS990S();
    }
  }
  
  // Serial2 input buffer handling
  while(Serial2.available() > 0)
  {
    char buffer[100];
    int size = Serial2.readBytesUntil(';', buffer, 100);        
    ProcessCommand(buffer, size);
  }
 
}

// Processes incoming commands from the TS-990S
void ProcessCommand(char rawcommand[], int commandLength)
{
  char command[commandLength];  
  arraycopy(rawcommand, command, commandLength);
  command[commandLength] = '\0';  
  String commandString = command;
  
  // Update when we received our last command
  LastCommandReceived = millis();
  PolledForData = false;
  ConnectPollingActive = false; 

  // Do we have a Valid Command?
  if (commandLength >= 2)
  {
      String commandDesignator = commandString.substring(0,2);

      if (commandDesignator == "00") // Radio Turned Off
      {
        #ifdef DEBUG
          Serial.println("Radio Turned off, polling active");
        #endif

        ResetSerialAndOutputs(); // Reset all our variables and band output
        ConnectPollingActive = true; // Start polling to reconnect if the radio gets turned back on        
      }
      else if (commandDesignator == "TB") // Transmit VFO
      {        
        // Get the TXing VFO number
        int newVFO = (int) commandString.substring(2, 3).toInt();

        #ifdef DEBUG
          Serial.print("VFO Changed:");
          Serial.println(newVFO);
        #endif

        if (newVFO != TXVFO)
        {
          TXVFO = newVFO;

          if (TXVFO == 0) {
            CurrentFreq = VFOACurrentFreq;          
          } else {
            CurrentFreq = VFOBCurrentFreq;          
          }

          SetOutputByFrequency(CurrentFreq);
        }
        

      } else if (commandDesignator == "FA") { // VFO A Frequency Changed
        #ifdef DEBUG
          Serial.print("VFO A Command:");
          Serial.println(commandString);
        #endif

        // Get the Frequency from the command
        String commandFreq = commandString.substring(2, 13);
        long newFreq = commandFreq.toInt();

        // If the frequency has changed then process it
        if (newFreq != VFOACurrentFreq)
        {
          VFOACurrentFreq = newFreq;

          #ifdef DEBUG
            Serial.print("VFO A New Freq:");
            Serial.println(VFOACurrentFreq);
          #endif 

          // Are we transmitting on this VFO?
          if (TXVFO == 0)
          {
            // Update the current output frequency
            CurrentFreq = VFOACurrentFreq;

            SetOutputByFrequency(CurrentFreq);            
          }          
        }
        
      } else if (commandDesignator == "FB") { // VFO B Frequency Changed
        #ifdef DEBUG
          Serial.print("VFO B Command:");
          Serial.println(commandString);
        #endif

        // Get the Frequency from the command
        String commandFreq = commandString.substring(2, 13);
        long newFreq = commandFreq.toInt();

        // If the frequency has changed then process it
        if (newFreq != VFOBCurrentFreq)
        {
          VFOBCurrentFreq = newFreq;

          #ifdef DEBUG
            Serial.print("VFO B New Freq:");
            Serial.println(VFOBCurrentFreq);
          #endif 

          // Are we transmitting on this VFO?
          if (TXVFO == 1)
          {
            // Update the current output frequency
            CurrentFreq = VFOBCurrentFreq;

            SetOutputByFrequency(CurrentFreq);            
          }          
        }
      }
  }
}

// Function to copy 'len' elements from 'src' to 'dst'
void arraycopy(char* src, char* dst, int len) {
    memcpy(dst, src, len);
}

// Sets the output by frequency
void SetOutputByFrequency(long Frequency)
{
  int Band = GetBandFromFrequency(Frequency);

  if (CurrentBand != Band)
  {
    CurrentBand = Band;
    #ifdef DEBUG
      Serial.print("Output changed to band:");
      Serial.println(CurrentBand);
    #endif

    SetOutputByBand(CurrentBand);
  }
}

// Sets the output pins from a given band
void SetOutputByBand(int Band)
{
  switch (Band)
  {
    case 10:
      digitalWrite(BCDOutput_A, HIGH);
      digitalWrite(BCDOutput_B, LOW);
      digitalWrite(BCDOutput_C, LOW);
      digitalWrite(BCDOutput_D, HIGH);
      break;
    case 15:
      digitalWrite(BCDOutput_A, HIGH);
      digitalWrite(BCDOutput_B, HIGH);
      digitalWrite(BCDOutput_C, HIGH);
      digitalWrite(BCDOutput_D, LOW);
      break;
    case 20:
      digitalWrite(BCDOutput_A, HIGH);
      digitalWrite(BCDOutput_B, LOW);
      digitalWrite(BCDOutput_C, HIGH);
      digitalWrite(BCDOutput_D, LOW);
      break;
    case 40:
      digitalWrite(BCDOutput_A, HIGH);
      digitalWrite(BCDOutput_B, HIGH);
      digitalWrite(BCDOutput_C, LOW);
      digitalWrite(BCDOutput_D, LOW);
      break;
    case 80:
      digitalWrite(BCDOutput_A, LOW);
      digitalWrite(BCDOutput_B, HIGH);
      digitalWrite(BCDOutput_C, LOW);
      digitalWrite(BCDOutput_D, LOW);
      break;
    case 160:
      digitalWrite(BCDOutput_A, HIGH);
      digitalWrite(BCDOutput_B, LOW);
      digitalWrite(BCDOutput_C, LOW);
      digitalWrite(BCDOutput_D, LOW);

      break;
    default: // Unknown Band - Set All outputs off
      digitalWrite(BCDOutput_A, LOW);
      digitalWrite(BCDOutput_B, LOW);
      digitalWrite(BCDOutput_C, LOW);
      digitalWrite(BCDOutput_D, LOW);
      break;
  }

}

// Gets the band in meters from a given Frequency in Hz
int GetBandFromFrequency(long Frequency)
{  

  if (Frequency >= 1810000 && Frequency <= 2000000) // 1810-2000    = 160m
    { return 160; }
  else if (Frequency >= 3500000 && Frequency <= 3800000) // 3500-3800    = 80m
    { return 80; }
  else if (Frequency >= 7000000 && Frequency <= 7200000) // 7000-7200    = 40m
    { return 40; }
  else if (Frequency >= 14000000 && Frequency <= 14350000) // 14000-14350  = 20m
    { return 20; }
  else if (Frequency >= 21000000 && Frequency <= 21450000) // 21000-21450  = 15m
    { return 15; }
  else if (Frequency >= 28000000 && Frequency <= 29700000) // 28000-29700  = 10m
    { return 10; }
  else { return 0; } // Unknown Band

}

