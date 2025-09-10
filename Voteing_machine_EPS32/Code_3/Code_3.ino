#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <vector>

LiquidCrystal_I2C lcd(0x27, 20, 4);

const char* ssid = "demo";
const char* password = "12345@#12345";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);


#define VOTE_PIN_1 27
#define VOTE_PIN_2 26
#define VOTE_PIN_3 25
#define VOTE_PIN_4 33
#define RESULT_PIN 14


struct Candidate {
  const char* name;
  int votes;
};


std::vector<Candidate> candidates = {
  { "BJP", 0 },
  { "NCP", 0 },
  { "MNS", 0 },
  { "Shiv Sena", 0 }
};

const int VOTE_THRESHOLD = 5;

WebServer server(80);

HardwareSerial gsmSerial(2);

struct VoteRecord {
  String candidate;
  int voteNumber;
  String dateTime;
};
std::vector<VoteRecord> voteRecords;

struct PendingVote {
  int candidateIndex;
  String phoneNumber;
  unsigned long timestamp;
  bool responded;
};
std::vector<PendingVote> pendingVotes;

bool waitingForSMS = false;
unsigned long smsWaitStartTime = 0;

void setup() {

  pinMode(VOTE_PIN_1, INPUT_PULLUP);
  pinMode(VOTE_PIN_2, INPUT_PULLUP);
  pinMode(VOTE_PIN_3, INPUT_PULLUP);
  pinMode(VOTE_PIN_4, INPUT_PULLUP);
  pinMode(RESULT_PIN, INPUT_PULLUP);

  Serial.begin(115200);

  gsmSerial.begin(9600, SERIAL_8N1, 16, 17);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Smart Voting Machine");
  delay(2000);

  displayCandidatesOnLcd();
  delay(2000);
  lcd.clear();

  setupWiFi();
  timeClient.begin();

  initializeGSM();

  server.on("/", handleRoot);
  server.on("/vote", handleVote);
  server.on("/results", handleResults);
  server.on("/download", handleDownload);
  server.begin();
}

void loop() {
  timeClient.update();
  server.handleClient();

  handleButtonPresses();

  updateLcdDisplay();

  checkSMSResponses();
  cleanupPendingVotes();
}


void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(5000);
  lcd.clear();
}

void initializeGSM() {
  sendGSMCommand("AT", "OK");
  sendGSMCommand("AT+CMGF=1", "OK");
  sendGSMCommand("AT+CNMI=2,2,0,0,0", "OK");
}

void sendGSMCommand(const char* cmd, const char* expected_response) {
  gsmSerial.println(cmd);
  unsigned long timeout = millis();
  while (millis() - timeout < 5000) {
    if (gsmSerial.available()) {
      String response = gsmSerial.readStringUntil('\n');
      if (response.indexOf(expected_response) != -1) {
        Serial.println("GSM Command success: " + response);
        return;
      }
    }
  }
  Serial.println("GSM Command failed: " + String(cmd));
}

void sendSMS(const String& phone, const String& msg) {
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phone);
  gsmSerial.print("\"\r\n");
  delay(500);
  gsmSerial.print(msg);
  gsmSerial.write(26);
  delay(1000);
}

void displayCandidatesOnLcd() {
  lcd.clear();
  for (int i = 0; i < candidates.size(); i++) {
    lcd.setCursor(0, i);
    lcd.print(String(i + 1) + ": " + candidates[i].name);
  }
}

void updateLcdDisplay() {
  for (int i = 0; i < candidates.size(); i++) {
    lcd.setCursor(0, i);
    lcd.print(candidates[i].name);
    lcd.setCursor(15, i);
    lcd.print("   ");
    lcd.setCursor(15, i);
    lcd.print(candidates[i].votes);
  }
}

void handleButtonPresses() {
  if (digitalRead(VOTE_PIN_1) == LOW) {
    processVote(0);
    waitRelease(VOTE_PIN_1);
  }
  if (digitalRead(VOTE_PIN_2) == LOW) {
    processVote(1);
    waitRelease(VOTE_PIN_2);
  }
  if (digitalRead(VOTE_PIN_3) == LOW) {
    processVote(2);
    waitRelease(VOTE_PIN_3);
  }
  if (digitalRead(VOTE_PIN_4) == LOW) {
    processVote(3);
    waitRelease(VOTE_PIN_4);
  }
  if (digitalRead(RESULT_PIN) == LOW) {
    displayAndSendResult();
    waitRelease(RESULT_PIN);
  }
}

void waitRelease(int pin) {
  while (digitalRead(pin) == LOW) { delay(10); }
  delay(250);  // Debounce delay
}

String getFormattedDateTime() {
  time_t epoch = timeClient.getEpochTime();
  struct tm* ptm = gmtime(&epoch);
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ptm);
  return String(buf);
}

void logVote(int candidateIndex, int num) {
  if (voteRecords.size() < 500) {
    voteRecords.push_back({ candidates[candidateIndex].name, num, getFormattedDateTime() });
  }
}

void processVote(int candidateIndex) {
  if (candidates[candidateIndex].votes == VOTE_THRESHOLD) {
    // Send SMS for 6th vote confirmation
    if (!waitingForSMS) {
      sendSMS("+917875751902", "Confirm 6th vote for " + String(candidates[candidateIndex].name) + "? Reply YES.");
      waitingForSMS = true;
      smsWaitStartTime = millis();
      // Add pending vote to list
      pendingVotes.push_back({ candidateIndex, "+91xxxxxxxxxx", millis(), false });
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Waiting SMS...");
      lcd.setCursor(0, 2);
      lcd.print("Reply YES");
    }
  } else {
    // Normal voting
    candidates[candidateIndex].votes++;
    logVote(candidateIndex, candidates[candidateIndex].votes);
  }
}

void checkSMSResponses() {
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    Serial.println("Received from GSM: " + response);
    // Check if the response contains an incoming SMS
    if (response.indexOf("+CMT:") != -1) {
      // Find the message content
      int startIndex = response.indexOf("\n") + 1;
      int endIndex = response.lastIndexOf("\n");
      if (startIndex > 0 && endIndex > startIndex) {
        String message = response.substring(startIndex, endIndex);
        message.trim();
        Serial.println("SMS Message: " + message);
        if (message.indexOf("YES") != -1 || message.indexOf("yes") != -1) {
          for (auto& pv : pendingVotes) {
            if (!pv.responded) {
              int confirmedIndex = pv.candidateIndex;
              candidates[confirmedIndex].votes++;
              logVote(confirmedIndex, candidates[confirmedIndex].votes);
              pv.responded = true;
              sendSMS(pv.phoneNumber, "6th vote confirmed for " + String(candidates[confirmedIndex].name));
              lcd.clear();
              lcd.setCursor(0, 1);
              lcd.print("Vote Confirmed!");
              delay(2000);
              waitingForSMS = false;
              break;
            }
          }
        }
      }
    }
  }
}

void cleanupPendingVotes() {
  unsigned long now = millis();
  for (size_t i = 0; i < pendingVotes.size(); ++i) {
    if (!pendingVotes[i].responded && (now - pendingVotes[i].timestamp) > 300000) {  // 5 minutes timeout
      Serial.println("Pending vote timed out.");
      pendingVotes.erase(pendingVotes.begin() + i);
      --i;
    }
  }
}

void displayAndSendResult() {
  int total = 0;
  for (const auto& candidate : candidates) {
    total += candidate.votes;
  }
  lcd.clear();
  String resultMsg;
  if (total == 0) {
    lcd.setCursor(0, 1);
    lcd.print("No Vote Cast Yet!");
    resultMsg = "No Votes Cast.";
  } else {
    int maxVotes = 0;
    for (const auto& candidate : candidates) {
      if (candidate.votes > maxVotes) {
        maxVotes = candidate.votes;
      }
    }

    String winners = "";
    int count = 0;
    for (const auto& candidate : candidates) {
      if (candidate.votes == maxVotes) {
        winners += String(candidate.name) + " ";
        count++;
      }
    }

    if (count == 1) {
      lcd.setCursor(0, 1);
      lcd.print("Winner:");
      lcd.setCursor(0, 2);
      lcd.print(winners);
      resultMsg = "Winner: " + winners + "(" + String(maxVotes) + " votes)";
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Tie Between:");
      lcd.setCursor(0, 2);
      lcd.print(winners);
      resultMsg = "Tie: " + winners;
    }
  }
  sendSMS("+91xxxxxxxxxx", resultMsg);
  delay(3000);
  lcd.clear();
  displayCandidatesOnLcd();  // Show candidate list after results
}

/* ==================== WEBSERVER PAGES ================== */

void handleRoot() {
  timeClient.update();
  unsigned long epoch = timeClient.getEpochTime();
  struct tm* ptm = gmtime((time_t*)&epoch);
  char dateStr[20], dayStr[15], timeStr[15];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", ptm);
  strftime(dayStr, sizeof(dayStr), "%A", ptm);
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", ptm);

  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>Smart Voting Machine</title>";
  html += "<style>";
  html += "body{font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f0f2f5; color: #333; margin: 0; padding: 20px; text-align: center;}";
  html += "h1{color: #0056b3; margin-bottom: 10px;}";
  html += "p{color: #666; font-size: 14px;}";
  html += ".container{max-width: 900px; margin: auto; padding: 20px; background: #fff; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.1); display: flex; flex-wrap: wrap; justify-content: center; gap: 20px;}";
  html += ".card{background-color: #e9f0f6; border-radius: 10px; padding: 25px; box-shadow: 0 2px 5px rgba(0,0,0,0.05); flex: 1 1 200px; transition: transform 0.2s, box-shadow 0.2s;}";
  html += ".card:hover{transform: translateY(-5px); box-shadow: 0 6px 15px rgba(0,0,0,0.1);}";
  html += "h2{color: #003366; margin-top: 0;}";
  html += ".votes{font-size: 2em; font-weight: bold; color: #4CAF50; margin: 10px 0;}";
  html += "button{width: 100%; padding: 12px; font-size: 16px; color: #fff; background-color: #007bff; border: none; border-radius: 5px; cursor: pointer; transition: background-color 0.3s, transform 0.1s;}";
  html += "button:hover{background-color: #0056b3;} button:active{transform: scale(0.98);}";
  html += ".link-btn{display: inline-block; padding: 10px 20px; margin: 10px; color: #fff; background-color: #5c677c; text-decoration: none; border-radius: 5px; transition: background-color 0.3s;}";
  html += ".link-btn:hover{background-color: #3b4252;}";
  html += "</style></head><body>";

  html += "<h1> Smart Voting System</h1>";
  html += "<p>Current Time: " + String(dateStr) + " | " + String(timeStr) + "</p>";

  html += "<div class='container'>";
  for (size_t i = 0; i < candidates.size(); ++i) {
    html += candidateBlock(candidates[i].name, candidates[i].votes, i);
  }
  html += "</div>";

  html += "<p><a href='/results' class='link-btn'>Show Final Results</a>";
  html += "<a href='/download' class='link-btn'>Download Vote Records</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

String candidateBlock(const char* name, int votes, int index) {
  String block = "<div class='card'>";
  block += "<h2>" + String(name) + "</h2>";
  block += "<p class='votes'>" + String(votes) + "</p>";
  block += "<form action='/vote' method='GET'>";
  block += "<input type='hidden' name='candidate' value='" + String(index) + "'>";
  block += "<button type='submit'>Vote</button></form></div>";
  return block;
}

void handleVote() {
  if (server.hasArg("candidate")) {
    int candidateIndex = server.arg("candidate").toInt();
    if (candidateIndex >= 0 && candidateIndex < candidates.size()) {
      processVote(candidateIndex);
      server.send(200, "text/plain", "Vote received.");
    } else {
      server.send(400, "text/plain", "Invalid candidate ID.");
    }
  } else {
    server.send(400, "text/plain", "Missing candidate ID.");
  }
}

void handleResults() {
  displayAndSendResult();
  server.send(200, "text/html", "<h2>Results have been sent!</h2><p>Check the LCD and your phone for details.</p><a href='/'>&larr; Back to Home</a>");
}

void handleDownload() {
  String csv = "Candidate,VoteNumber,DateTime\n";
  for (const auto& record : voteRecords) {
    csv += "\"" + record.candidate + "\"," + String(record.voteNumber) + ",\"" + record.dateTime + "\"\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=votes.csv");
  server.send(200, "text/csv", csv);
}