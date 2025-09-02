#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>  

LiquidCrystal_I2C lcd(0x27, 20, 4);

const char* ssid = "Prateek";
const char* password = "justdoelectronics@#12345";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

#define v1 27
#define v2 26
#define v3 25
#define v4 33
#define v5 14

String candidate1 = "Narendra Modi";
String candidate2 = "Rahul Gandhi";
String candidate3 = "Shiv Sena";
String candidate4 = "Mamata Banerjee";

int vote1 = 0, vote2 = 0, vote3 = 0, vote4 = 0;

HardwareSerial gsmSerial(2);
WebServer server(80);

struct VoteRecord {
  String candidate;
  int voteNumber;
  String dateTime;
};
VoteRecord voteRecords[500];
int recordCount = 0;

void setup() {
  pinMode(v1, INPUT_PULLUP);
  pinMode(v2, INPUT_PULLUP);
  pinMode(v3, INPUT_PULLUP);
  pinMode(v4, INPUT_PULLUP);
  pinMode(v5, INPUT_PULLUP);

  Serial.begin(115200);
  gsmSerial.begin(9600, SERIAL_8N1, 16, 17);

  lcd.init();
  lcd.backlight();

  initializeGSM();

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Smart Voting Machine");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1: " + candidate1);
  lcd.setCursor(0, 1);
  lcd.print("2: " + candidate2);
  lcd.setCursor(0, 2);
  lcd.print("3: " + candidate3);
  lcd.setCursor(0, 3);
  lcd.print("4: " + candidate4);
  delay(2000);
  lcd.clear();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  timeClient.begin();
  timeClient.setTimeOffset(19800); 

  server.on("/", handleRoot);
  server.on("/vote1", []() {
    handleVote(1);
  });
  server.on("/vote2", []() {
    handleVote(2);
  });
  server.on("/vote3", []() {
    handleVote(3);
  });
  server.on("/vote4", []() {
    handleVote(4);
  });
  server.on("/results", handleResults);
  server.on("/download", handleDownload);
  server.begin();
}

void loop() {
  timeClient.update();

  lcd.setCursor(0, 0);
  lcd.print(candidate1.substring(0, 12) + "   ");
  lcd.setCursor(15, 0);
  lcd.print(vote1);
  lcd.setCursor(0, 1);
  lcd.print(candidate2.substring(0, 12) + "   ");
  lcd.setCursor(15, 1);
  lcd.print(vote2);
  lcd.setCursor(0, 2);
  lcd.print(candidate3.substring(0, 12) + "   ");
  lcd.setCursor(15, 2);
  lcd.print(vote3);
  lcd.setCursor(0, 3);
  lcd.print(candidate4.substring(0, 12) + "   ");
  lcd.setCursor(15, 3);
  lcd.print(vote4);

  if (digitalRead(v1) == LOW) {
    vote1++;
    logVote(candidate1, vote1);
    sendVoteSMS(candidate1, vote1);
    waitRelease(v1);
  }
  if (digitalRead(v2) == LOW) {
    vote2++;
    logVote(candidate2, vote2);
    sendVoteSMS(candidate2, vote2);
    waitRelease(v2);
  }
  if (digitalRead(v3) == LOW) {
    vote3++;
    logVote(candidate3, vote3);
    sendVoteSMS(candidate3, vote3);
    waitRelease(v3);
  }
  if (digitalRead(v4) == LOW) {
    vote4++;
    logVote(candidate4, vote4);
    sendVoteSMS(candidate4, vote4);
    waitRelease(v4);
  }
  if (digitalRead(v5) == LOW) {
    displayAndSendResult();
    waitRelease(v5);
  }

  server.handleClient();
}

void waitRelease(int pin) {
  while (digitalRead(pin) == LOW) delay(10);
  delay(250);
}

void initializeGSM() {
  sendGSMCommand("AT");
  delay(100);
  sendGSMCommand("AT+CMGF=1");
  delay(100);
}

void sendGSMCommand(const char* cmd) {
  gsmSerial.println(cmd);
  delay(900);
  while (gsmSerial.available()) Serial.write(gsmSerial.read());
}

void sendVoteSMS(String candidate, int count) {
  sendGSMCommand("AT+CMGS=\"+9199756174xx\"");  
  delay(350);
  gsmSerial.print("Vote cast for ");
  gsmSerial.print(candidate);
  gsmSerial.print(". Total: ");
  gsmSerial.print(count);
  delay(350);
  gsmSerial.write(26);
  delay(1500);
}

void sendResultSMS(String msg) {
  sendGSMCommand("AT+CMGS=\"+9199756174xx\"");  
  delay(350);
  gsmSerial.print(msg);
  delay(350);
  gsmSerial.write(26);
  delay(1500);
}

void logVote(String candidate, int num) {
  if (recordCount < 500) {
    voteRecords[recordCount].candidate = candidate;
    voteRecords[recordCount].voteNumber = num;
    voteRecords[recordCount].dateTime = getFormattedDateTime();
    recordCount++;
  }
}

String getFormattedDateTime() {
  time_t epoch = timeClient.getEpochTime() + 19800;  
  struct tm* ptm = gmtime(&epoch);

  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%d %A %H:%M:%S", ptm);
  return String(buf);
}

void displayAndSendResult() {
  int total = vote1 + vote2 + vote3 + vote4;
  lcd.clear();
  if (total == 0) {
    lcd.setCursor(0, 1);
    lcd.print("No Vote Cast Yet!");
    sendResultSMS("No votes cast yet.");
  } else {
    int maxVotes = max(max(vote1, vote2), max(vote3, vote4));
    String winners = "";
    int countWinners = 0;
    if (vote1 == maxVotes) {
      winners += candidate1 + " ";
      countWinners++;
    }
    if (vote2 == maxVotes) {
      winners += candidate2 + " ";
      countWinners++;
    }
    if (vote3 == maxVotes) {
      winners += candidate3 + " ";
      countWinners++;
    }
    if (vote4 == maxVotes) {
      winners += candidate4 + " ";
      countWinners++;
    }
    if (countWinners == 1) {
      lcd.setCursor(0, 1);
      lcd.print("Winner:");
      lcd.setCursor(0, 2);
      lcd.print(winners);
      sendResultSMS(winners + " wins with " + String(maxVotes) + " votes.");
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Result: Tie");
      lcd.setCursor(0, 2);
      lcd.print("Between:");
      lcd.setCursor(0, 3);
      lcd.print(winners);
      sendResultSMS("Vote tied between: " + winners);
    }
  }
  delay(3000);
  vote1 = vote2 = vote3 = vote4 = 0;
  lcd.clear();
}

void handleRoot() {
  timeClient.update();

  unsigned long epoch = timeClient.getEpochTime() + 19800;  
  struct tm* ptm = gmtime((time_t*)&epoch);

  char dateStr[20];
  char dayStr[15];
  char timeStr[15];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", ptm);
  strftime(dayStr, sizeof(dayStr), "%A", ptm);
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", ptm);

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Smart Voting Machine</title>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += R"rawliteral(
    <style>
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        background: linear-gradient(135deg, #667eea, #764ba2);
        color: white;
        margin: 0; padding: 0; min-height: 100vh;
        display: flex; flex-direction: column; align-items: center;
      }
      h1 {
        margin: 25px 0 10px;
        letter-spacing: 3px;
        text-shadow: 2px 2px 5px #0008;
      }
      .timeDisplay {
        font-weight: bold;
        background: rgba(0,0,0,0.3);
        padding: 10px 25px;
        border-radius: 25px;
        margin-bottom: 30px;
        font-size: 22px;
        box-shadow: 0 0 15px #0008;
      }
      .candidates {
        display: flex;
        flex-wrap: wrap;
        justify-content: center;
        gap: 25px;
        max-width: 960px;
        width: 100%;
        padding-bottom: 30px;
      }
      .candidate-card {
        background: rgba(255, 255, 255, 0.15);
        border-radius: 20px;
        box-shadow: 0 8px 20px rgba(0,0,0,0.3);
        width: 220px;
        padding: 25px 20px;
        text-align: center;
        transition: transform 0.3s ease, background 0.3s ease;
        cursor: pointer;
        user-select: none;
      }
      .candidate-card:hover {
        background: rgba(255, 255, 255, 0.3);
        transform: scale(1.05);
        box-shadow: 0 15px 25px rgba(0,0,0,0.5);
      }
      .candidate-name {
        font-size: 20px;
        font-weight: 700;
        margin-bottom: 15px;
        text-shadow: 1px 1px 3px #0006;
      }
      .vote-count {
        font-size: 28px;
        font-weight: 800;
        margin-bottom: 15px;
        color: #ffd700;
        text-shadow: 1px 1px 3px #000;
      }
      .vote-button {
        background: #4caf50;
        border: none;
        border-radius: 40px;
        color: white;
        font-size: 18px;
        font-weight: 700;
        padding: 12px 35px;
        cursor: pointer;
        box-shadow: 0 6px 12px rgba(0, 0, 0, 0.2);
        transition: background-color 0.3s ease;
        user-select: none;
      }
      .vote-button:hover {
        background: #45a049;
      }
      .progress-bar {
        background: rgba(255, 255, 255, 0.4);
        height: 22px;
        border-radius: 15px;
        overflow: hidden;
        margin-top: 15px;
        box-shadow: inset 0 0 7px rgba(0, 0, 0, 0.2);
      }
      .progress-bar-fill {
        background: linear-gradient(90deg, #ffd700, #ffa500);
        height: 100%;
        width: 0%;
        text-align: right;
        padding-right: 10px;
        font-weight: bold;
        color: #212121;
        border-radius: 15px 0 0 15px;
        box-sizing: border-box;
        transition: width 0.5s ease-in-out;
      }
      .notification {
        background: rgba(0, 128, 0, 0.7);
        padding: 15px 25px;
        font-size: 20px;
        border-radius: 25px;
        margin: 30px auto;
        max-width: 600px;
        box-shadow: 0 5px 20px rgba(0, 0, 0, 0.75);
      }
      .bottom-buttons {
        margin-bottom: 40px;
        display: flex;
        gap: 25px;
        justify-content: center;
        width: 100%;
      }
      .bottom-buttons a {
        text-decoration: none;
      }
      .result-button, .download-button {
        background: #2196f3;
        color: white;
        font-size: 18px;
        font-weight: 700;
        padding: 12px 40px;
        border-radius: 40px;
        box-shadow: 0 6px 12px rgba(0, 0, 0, 0.3);
        display: inline-block;
        transition: background-color 0.3s ease;
        cursor: pointer;
        user-select: none;
      }
      .result-button:hover, .download-button:hover {
        background: #0b7dda;
      }
    </style>
  )rawliteral";

  html += "</head><body>";
  html += "<h1>Smart Voting Machine</h1>";
  html += "<div class='timeDisplay'>Date: " + String(dateStr) + "  &nbsp;&nbsp;  Day: " + String(dayStr) + "  &nbsp;&nbsp;  Time: " + String(timeStr) + "</div>";

  int maxBarVotes = 20;
  auto candidateBlock = [&](const String& name, int votes, int idx) {
    int barWidthPercent = (votes > maxBarVotes) ? 100 : (votes * 100) / maxBarVotes;
    String block = "<div class='candidate-card'>";
    block += "<div class='candidate-name'>" + name + "</div>";
    block += "<div class='vote-count'>" + String(votes) + " votes</div>";
    block += "<button class='vote-button' onclick=\"location.href='/vote" + String(idx) + "'\">Vote</button>";
    block += "<div class='progress-bar'><div class='progress-bar-fill' style='width:" + String(barWidthPercent) + "%'></div></div>";
    block += "</div>";
    return block;
  };

  html += "<div class='candidates'>";
  html += candidateBlock(candidate1, vote1, 1);
  html += candidateBlock(candidate2, vote2, 2);
  html += candidateBlock(candidate3, vote3, 3);
  html += candidateBlock(candidate4, vote4, 4);
  html += "</div>";

  if (vote1 > 5 && vote2 > 5 && vote3 > 5 && vote4 > 5) {
    html += "<div class='notification'>Notification: All candidates have more than 5 votes.</div>";
  }
  html += "<div class='bottom-buttons'>";
  html += "<a href='/results' class='result-button'>Display & Send Result</a>";
  html += "<a href='/download' class='download-button'>Download CSV</a>";
  html += "</div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleVote(int candidate) {
  switch (candidate) {
    case 1:
      vote1++;
      logVote(candidate1, vote1);
      sendVoteSMS(candidate1, vote1);
      break;
    case 2:
      vote2++;
      logVote(candidate2, vote2);
      sendVoteSMS(candidate2, vote2);
      break;
    case 3:
      vote3++;
      logVote(candidate3, vote3);
      sendVoteSMS(candidate3, vote3);
      break;
    case 4:
      vote4++;
      logVote(candidate4, vote4);
      sendVoteSMS(candidate4, vote4);
      break;
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleResults() {
  displayAndSendResult();
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Voting Results</title></head><body><h2>Results displayed on LCD and SMS sent.</h2>";
  html += "<p><a href='/'>Back to Voting</a></p></body></html>";
  server.send(200, "text/html", html);
}

void handleDownload() {
  String csv = "Candidate,VoteNumber,DateTime\n";
  for (int i = 0; i < recordCount; i++) {
    csv += "\"" + voteRecords[i].candidate + "\"," + String(voteRecords[i].voteNumber) + ",\"" + voteRecords[i].dateTime + "\"\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=voting_records.csv");
  server.send(200, "text/csv", csv);
}
