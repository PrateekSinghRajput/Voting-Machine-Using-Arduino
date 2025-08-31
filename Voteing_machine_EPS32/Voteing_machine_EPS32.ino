#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD 20x4
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Button Pins for hardware voting
#define v1 27
#define v2 26
#define v3 25
#define v4 33
#define v5 14 

// Vote counters
int vote1 = 0;
int vote2 = 0;
int vote3 = 0;
int vote4 = 0;

// Candidate names
String candidate1 = "Narendra Modi";
String candidate2 = "Rahul Gandhi";
String candidate3 = "Shiv Sena";
String candidate4 = "Mamata Banerjee";

// GSM serial using UART2
HardwareSerial gsmSerial(2);

// WiFi credentials - change to actual
const char* ssid = "Prateek";
const char* password = "justdoelectronics@#12345";

WebServer server(80);

void setup() {
  pinMode(v1, INPUT_PULLUP);
  pinMode(v2, INPUT_PULLUP);
  pinMode(v3, INPUT_PULLUP);
  pinMode(v4, INPUT_PULLUP);
  pinMode(v5, INPUT_PULLUP);

  Serial.begin(115200);
  gsmSerial.begin(9600, SERIAL_8N1, 16, 17);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  // Initialize GSM
  initializeGSM();

  // Welcome message
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Smart Voting Machine");
  delay(4000);
  lcd.clear();

  // Show candidate names
  lcd.setCursor(0, 0); lcd.print("1: " + candidate1);
  lcd.setCursor(0, 1); lcd.print("2: " + candidate2);
  lcd.setCursor(0, 2); lcd.print("3: " + candidate3);
  lcd.setCursor(0, 3); lcd.print("4: " + candidate4);
  delay(4000);
  lcd.clear();

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // Web server routes
  server.on("/", handleRoot);
  server.on("/vote1", []() { voteCandidate(1); });
  server.on("/vote2", []() { voteCandidate(2); });
  server.on("/vote3", []() { voteCandidate(3); });
  server.on("/vote4", []() { voteCandidate(4); });
  server.on("/results", handleResults);

  server.begin();
}

void loop() {
  // Update LCD with votes
  lcd.setCursor(0, 0); lcd.print(candidate1.substring(0,12) + "   ");
  lcd.setCursor(15, 0); lcd.print(vote1);
  lcd.setCursor(0, 1); lcd.print(candidate2.substring(0,12) + "   ");
  lcd.setCursor(15, 1); lcd.print(vote2);
  lcd.setCursor(0, 2); lcd.print(candidate3.substring(0,12) + "   ");
  lcd.setCursor(15, 2); lcd.print(vote3);
  lcd.setCursor(0, 3); lcd.print(candidate4.substring(0,12) + "   ");
  lcd.setCursor(15, 3); lcd.print(vote4);

  // Hardware button voting
  if (digitalRead(v1) == LOW) { vote1++; sendVoteSMS(candidate1, vote1); waitForButtonRelease(v1); }
  if (digitalRead(v2) == LOW) { vote2++; sendVoteSMS(candidate2, vote2); waitForButtonRelease(v2); }
  if (digitalRead(v3) == LOW) { vote3++; sendVoteSMS(candidate3, vote3); waitForButtonRelease(v3); }
  if (digitalRead(v4) == LOW) { vote4++; sendVoteSMS(candidate4, vote4); waitForButtonRelease(v4); }

  // Display and send results hardware button
  if (digitalRead(v5) == LOW) { displayAndSendResult(); waitForButtonRelease(v5); }

  // Handle web client
  server.handleClient();
}

void waitForButtonRelease(int pin) {
  while (digitalRead(pin) == LOW) delay(10);
  delay(300); // debounce
}

void initializeGSM() {
  sendGSMCommand("AT");
  delay(100);
  sendGSMCommand("AT+CMGF=1");
  delay(100);
}

void sendGSMCommand(const char* cmd) {
  gsmSerial.println(cmd);
  delay(1000);
  while(gsmSerial.available()) Serial.write(gsmSerial.read());
}

void sendVoteSMS(String candidate, int count) {
  Serial.println("Sending vote SMS...");
  sendGSMCommand("AT+CMGS=\"+918830584864\""); // Replace number
  delay(500);
  gsmSerial.print("Vote cast for ");
  gsmSerial.print(candidate);
  gsmSerial.print(". Total: ");
  gsmSerial.print(count);
  delay(500);
  gsmSerial.write(26);
  delay(3000);
}

void displayAndSendResult() {
  int total = vote1 + vote2 + vote3 + vote4;
  lcd.clear();

  if(total == 0) {
    lcd.setCursor(0,1); lcd.print("No Vote Cast Yet!");
    sendResultSMS("No votes cast yet.");
  } else {
    int maxVotes = max(max(vote1,vote2), max(vote3,vote4));
    String winners = "";
    int countWinners = 0;

    if(vote1 == maxVotes) { winners += candidate1 + " "; countWinners++; }
    if(vote2 == maxVotes) { winners += candidate2 + " "; countWinners++; }
    if(vote3 == maxVotes) { winners += candidate3 + " "; countWinners++; }
    if(vote4 == maxVotes) { winners += candidate4 + " "; countWinners++; }

    if(countWinners == 1) {
      lcd.setCursor(0,1); lcd.print("Winner:");
      lcd.setCursor(0,2); lcd.print(winners);
      sendResultSMS(winners + " wins with " + String(maxVotes) + " votes.");
    } else {
      lcd.setCursor(0,1); lcd.print("Result: Tie");
      lcd.setCursor(0,2); lcd.print("Between:");
      lcd.setCursor(0,3); lcd.print(winners);
      sendResultSMS("Vote tied between: " + winners);
    }
  }
  delay(6000);
  vote1 = vote2 = vote3 = vote4 = 0; // Reset
  lcd.clear();
}

void sendResultSMS(String text) {
  Serial.println("Sending result SMS...");
  sendGSMCommand("AT+CMGS=\"+918830584864\""); // Replace number
  delay(500);
  gsmSerial.print(text);
  delay(500);
  gsmSerial.write(26);
  delay(3000);
}

// Handle root "/" with graphical UI and voting buttons
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
   html += "<meta http-equiv=\"refresh\" content=\"5\">";
  html += "<title>Smart Voting Machine</title>";
  html += R"rawliteral(
    <style>
      body { font-family: Arial, sans-serif; text-align:center; margin:10px; background:#f4f4f9; }
      h1 { color:#333; }
      .candidate { background:#fff; margin:10px auto; padding:15px; max-width:400px; border-radius:8px; box-shadow:0 0 7px rgba(0,0,0,0.1); }
      button.voteBtn {
        background:#4CAF50; border:none; color:#fff; padding:12px 25px; font-size:18px; border-radius:6px; cursor:pointer; margin-top:8px; transition:background-color 0.3s ease;
      }
      button.voteBtn:hover { background:#45a049; }
      .progress-bar-bg {
        background:#ddd; border-radius:5px; margin-top:12px; height:20px; width:100%;
      }
      .progress-bar-fill {
        background:#4CAF50; height:100%; border-radius:5px; text-align:right; padding-right:8px; color:#fff; font-weight:bold; line-height:20px;
      }
      .notification {
        padding:15px; background:#dff0d8; color:#3c763d; margin:20px auto; width:90%; max-width:400px; border-radius:8px; box-shadow:0 0 7px rgba(0,0,0,0.1); font-size:18px;
      }
      a.buttonLink { text-decoration:none; }
      .resultBtn { background:#2196F3; margin-top:25px; font-weight:bold; }
      .resultBtn:hover { background:#0b7dda; }
    </style>
  )rawliteral";
  html += "</head><body>";
  html += "<h1>Smart Voting Machine</h1>";

  int maxBarVotes = 20;
  auto candidateBlock = [&](const String& name, int votes, int idx) {
    int barWidthPercent = (votes > maxBarVotes) ? 100 : (votes * 100) / maxBarVotes;
    String block = "<div class='candidate'>";
    block += "<h2>" + name + "</h2>";
    block += "<a class='buttonLink' href='/vote" + String(idx) + "'>";
    block += "<button class='voteBtn'>Vote</button></a>";
    block += "<div class='progress-bar-bg'>";
    block += "<div class='progress-bar-fill' style='width:" + String(barWidthPercent) + "%'>" + String(votes) + "</div>";
    block += "</div></div>";
    return block;
  };

  html += candidateBlock(candidate1, vote1, 1);
  html += candidateBlock(candidate2, vote2, 2);
  html += candidateBlock(candidate3, vote3, 3);
  html += candidateBlock(candidate4, vote4, 4);

  if (vote1 > 5 && vote2 > 5 && vote3 > 5 && vote4 > 5) {
    html += "<div class='notification'>Notification: All candidates have more than 5 votes.</div>";
  }

  html += "<a class='buttonLink' href='/results'><button class='voteBtn resultBtn'>Display & Send Result</button></a>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Vote handler from web buttons
void voteCandidate(int candidate) {
  switch(candidate) {
    case 1: vote1++; sendVoteSMS(candidate1, vote1); break;
    case 2: vote2++; sendVoteSMS(candidate2, vote2); break;
    case 3: vote3++; sendVoteSMS(candidate3, vote3); break;
    case 4: vote4++; sendVoteSMS(candidate4, vote4); break;
  }
  // Redirect back to root to refresh
  server.sendHeader("Location", "/");
  server.send(303);
}

// Results page handler
void handleResults() {
  displayAndSendResult();
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Voting Results</title></head><body><h2>Results displayed on LCD and SMS sent.</h2>";
  html += "<p><a href='/'>Back to Voting</a></p></body></html>";
  server.send(200, "text/html", html);
}
