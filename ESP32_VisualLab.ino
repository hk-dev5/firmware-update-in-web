#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>

// Cấu hình chân BOOT trên ESP32-C3 Super Mini thường là GPIO 9 (mức active LOW khi nhấn)
const int BOOT_BUTTON_PIN = 9;

const char* ssid = "ESP32-C3-IDE";
const char* password = "C3ide123";

WebServer server(80);

// Trạng thái hệ thống: 0 = PRACTICE, 1 = OTA_READY, 2 = OTA_UPDATING
volatile int systemMode = 0; 
unsigned long buttonPressStartTime = 0;
bool buttonWasPressed = false;

// Giao diện Web IDE tích hợp đầy đủ các tính năng kéo thả lệnh, tìm kiếm thư viện, debug và upload .bin
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-C3 Advanced Web IDE</title>
    <style>
        :root { --primary: #007acc; --bg: #f4f6f9; --surface: #ffffff; --text: #333333; --border: #d1d5db; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 15px; }
        .container { max-width: 1200px; margin: auto; display: grid; grid-template-columns: 280px 1fr; gap: 20px; }
        header { grid-column: 1 / -1; background: var(--surface); padding: 15px 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.05); display: flex; justify-content: space-between; align-items: center; }
        h2 { color: var(--primary); margin: 0; }
        .sidebar { background: var(--surface); padding: 15px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.05); height: fit-content; }
        .main-content { display: flex; flex-direction: column; gap: 20px; }
        .card { background: var(--surface); padding: 15px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }
        textarea { width: 100%; height: 300px; font-family: Consolas, monospace; font-size: 13px; padding: 10px; border: 1px solid var(--border); border-radius: 4px; box-sizing: border-box; background: #1e1e1e; color: #dcdcdc; resize: vertical; }
        input, select, button { padding: 8px 12px; font-size: 14px; border: 1px solid var(--border); border-radius: 4px; margin-top: 5px; }
        button { background: var(--primary); color: white; border: none; cursor: pointer; font-weight: 600; transition: background 0.2s; }
        button:hover { background: #005999; }
        button.success { background: #28a745; }
        button.success:hover { background: #218838; }
        .command-item { background: #e3f2fd; border: 1px solid #90caf9; padding: 6px 10px; margin-bottom: 6px; border-radius: 4px; font-size: 13px; cursor: grab; user-select: none; }
        .command-item:active { cursor: grabbing; }
        pre { background: #1e1e1e; color: #4ec9b0; padding: 10px; border-radius: 4px; overflow-x: auto; max-height: 180px; font-size: 12px; margin: 0; }
        .drop-zone { border: 2px dashed var(--primary); border-radius: 6px; padding: 20px; text-align: center; background: #f8fbfe; cursor: pointer; }
        .flex { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
        .tab-buttons { display: flex; gap: 5px; margin-bottom: 10px; }
        .tab-btn { background: #e0e0e0; color: #333; border: none; padding: 6px 12px; border-radius: 4px; cursor: pointer; }
        .tab-btn.active { background: var(--primary); color: white; }
    </style>
</head>
<body>
<div class="container">
    <header>
        <h2>ESP32-C3 Web IDE & OTA</h2>
        <div>
            <span id="deviceStatus" style="font-weight: bold; color: #28a745;">Trạng thái: OTA Ready</span>
            <button onclick="exitOtaMode()" style="background: #dc3545; margin-left: 10px;">Thoát chế độ OTA</button>
        </div>
    </header>

    <!-- Sidebar: Thư viện lệnh kéo thả & Library Explorer -->
    <div class="sidebar">
        <h3>Kho Lệnh Kéo Thả</h3>
        <div class="tab-buttons">
            <button class="tab-btn active" onclick="switchTab('builtin')">Cơ bản</button>
            <button class="tab-btn" onclick="switchTab('explorer')">Thư viện</button>
        </div>

        <div id="tab-builtin">
            <p style="font-size: 12px; color: #666;">Kéo lệnh vào vùng soạn thảo:</p>
            <div class="command-item" draggable="true" ondragstart="drag(event)" data-code="pinMode(8, OUTPUT);">Cấu hình GPIO (pinMode)</div>
            <div class="command-item" draggable="true" ondragstart="drag(event)" data-code="digitalWrite(8, HIGH);">Bật GPIO (digitalWrite HIGH)</div>
            <div class="command-item" draggable="true" ondragstart="drag(event)" data-code="digitalWrite(8, LOW);">Tắt GPIO (digitalWrite LOW)</div>
            <div class="command-item" draggable="true" ondragstart="drag(event)" data-code="delay(1000);">Đợi thời gian (delay)</div>
            <div class="command-item" draggable="true" ondragstart="drag(event)" data-code="Serial.begin(115200);">Khởi động Serial</div>
            <div class="command-item" draggable="true" ondragstart="drag(event)" data-code="WiFi.begin(&quot;SSID&quot;, &quot;PASS&quot;);">Kết nối Wi-Fi</div>
        </div>

        <div id="tab-explorer" style="display: none;">
            <p style="font-size: 12px; color: #666;">Tìm kiếm API từ thư viện mạng:</p>
            <input type="text" id="libSearchQuery" placeholder="VD: ArduinoJson, FastLED..." style="width: 100%;">
            <button onclick="searchLibrary()" style="width: 100%; margin-top: 5px;">Tra cứu API</button>
            <div id="libSearchResults" style="margin-top: 10px; max-height: 250px; overflow-y: auto;"></div>
        </div>
    </div>

    <!-- Main Content Area -->
    <div class="main-content">
        <!-- 1. Code Workspace -->
        <div class="card">
            <h3>1. Code Workspace (.ino)</h3>
            <div class="flex" style="margin-bottom: 10px;">
                <button onclick="saveLocal()">Lưu tạm</button>
                <button onclick="exportIno()">Tải file .ino</button>
                <button onclick="loadDefault()">Mẫu chuẩn</button>
            </div>
            <textarea id="codeEditor" ondragover="allowDrop(event)" ondrop="drop(event)"></textarea>
        </div>

        <!-- 2. Review & Debug -->
        <div class="card">
            <h3>2. Review & Debug</h3>
            <div class="flex">
                <button onclick="runStaticChecks()">Kiểm tra lỗi tĩnh (Run Checks)</button>
            </div>
            <pre id="debugLog">Chưa có thông báo lỗi...</pre>
        </div>

        <!-- 3. Firmware Upload (.bin) -->
        <div class="card">
            <h3>3. Firmware Upload (.bin) & OTA</h3>
            <p style="font-size: 13px; color: #666;">Kéo thả file biên dịch .bin từ máy tính vào đây sau khi dùng Arduino CLI:</p>
            <div class="drop-zone" id="binDropZone" onclick="document.getElementById('binFile').click()">
                Drop compiled firmware .bin here <br><small>(Hoặc bấm để chọn file)</small>
                <input type="file" id="binFile" accept=".bin" style="display: none;" onchange="handleBinSelect(this)">
            </div>
            <p id="binFileInfo" style="font-size: 13px; font-weight: bold; margin-top: 8px;"></p>
            <button onclick="uploadFirmwareOta()" class="success" style="width: 100%; margin-top: 10px;">Upload Firmware OTA</button>
            <pre id="uploadLog" style="margin-top: 10px;">Tiến trình nạp firmware sẽ hiển thị ở đây...</pre>
        </div>
    </div>
</div>

<script>
    const defaultCode = `#include <Arduino.h>\n\nconst int LED_PIN = 8;\n\nvoid setup() {\n  pinMode(LED_PIN, OUTPUT);\n  Serial.begin(115200);\n}\n\nvoid loop() {\n  digitalWrite(LED_PIN, HIGH);\n  delay(500);\n  digitalWrite(LED_PIN, LOW);\n  delay(500);\n}`;

    let selectedBinFile = null;

    window.onload = function() {
        const saved = localStorage.getItem('esp32_code');
        document.getElementById('codeEditor').value = saved ? saved : defaultCode;
    };

    function saveLocal() {
        localStorage.setItem('esp32_code', document.getElementById('codeEditor').value);
        alert('Đã lưu code vào bộ nhớ trình duyệt!');
    }

    function loadDefault() {
        if(confirm('Ghi đè code mẫu?')) document.getElementById('codeEditor').value = defaultCode;
    }

    function exportIno() {
        const code = document.getElementById('codeEditor').value;
        const blob = new Blob([code], { type: 'text/plain;charset=utf-8' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'ESP32C3_Project.ino';
        a.click();
        URL.revokeObjectURL(url);
    }

    function switchTab(tab) {
        document.getElementById('tab-builtin').style.display = tab === 'builtin' ? 'block' : 'none';
        document.getElementById('tab-explorer').style.display = tab === 'explorer' ? 'block' : 'none';
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        event.target.classList.add('active');
    }

    function allowDrop(ev) { ev.preventDefault(); }
    function drag(ev) { ev.dataTransfer.setData("text", ev.target.dataset.code); }
    function drop(ev) {
        ev.preventDefault();
        const data = ev.dataTransfer.getData("text");
        const textarea = document.getElementById('codeEditor');
        const pos = textarea.selectionStart;
        textarea.value = textarea.value.substring(0, pos) + "\n  " + data + "\n" + textarea.value.substring(pos);
    }

    async function searchLibrary() {
        const query = document.getElementById('libSearchQuery').value.trim();
        const resultsDiv = document.getElementById('libSearchResults');
        if (!query) return;
        resultsDiv.innerHTML = "Đang tra cứu trực tuyến...";
        try {
            // Truy vấn thông tin thư viện qua API Arduino Library Index / GitHub Registry công khai
            let res = await fetch(`https://api.github.com/search/repositories?q=${query}+arduino`);
            let data = await res.json();
            if (data.items && data.items.length > 0) {
                let html = "";
                data.items.slice(0, 3).forEach(repo => {
                    html += `<div style="background:#fff; border:1px solid #ccc; padding:6px; margin-bottom:5px; border-radius:4px;">
                        <b>${repo.name}</b><br><small>${repo.description || 'Không có mô tả'}</small><br>
                        <button onclick="addLibCommand('#include <${repo.name}.h>')">Thêm #include</button>
                    </div>`;
                });
                resultsDiv.innerHTML = html;
            } else {
                resultsDiv.innerHTML = "Không tìm thấy thư viện phù hợp.";
            }
        } catch(e) {
            resultsDiv.innerHTML = "Lỗi kết nối mạng khi tìm kiếm thư viện.";
        }
    }

    function addLibCommand(incStr) {
        const textarea = document.getElementById('codeEditor');
        textarea.value = incStr + "\n" + textarea.value;
        alert("Đã thêm khai báo thư viện vào đầu file.");
    }

    function runStaticChecks() {
        const code = document.getElementById('codeEditor').value;
        let logs = [];
        if (!code.includes('void setup()')) logs.push("[LỖI] Thiếu hàm bắt buộc: void setup()");
        if (!code.includes('void loop()')) logs.push("[LỖI] Thiếu hàm bắt buộc: void loop()");
        if ((code.split('{').length - 1) !== (code.split('}').length - 1)) logs.push("[CẢNH BÁO] Số lượng dấu mở ngoặc '{' và đóng ngoặc '}' không khớp!");
        if (code.includes('pinMode') && !code.includes('OUTPUT') && !code.includes('INPUT')) logs.push("[GỢI Ý] Kiểm tra lại tham số chân trong pinMode.");
        
        if (logs.length === 0) {
            document.getElementById('debugLog').textContent = "Kiểm tra tĩnh thành công! Không phát hiện lỗi cấu trúc cơ bản.";
        } else {
            document.getElementById('debugLog').textContent = logs.join("\n");
        }
    }

    function handleBinSelect(input) {
        if (input.files.length > 0) {
            selectedBinFile = input.files[0];
            document.getElementById('binFileInfo').textContent = `File: ${selectedBinFile.name} (${selectedBinFile.size} bytes)`;
        }
    }

    async function uploadFirmwareOta() {
        if (!selectedBinFile) {
            alert("Vui lòng chọn hoặc kéo thả file .bin trước khi nạp!");
            return;
        }
        if (!confirm("Xác nhận nạp firmware mới qua Wi-Fi OTA? Thiết bị sẽ tự khởi động lại sau khi hoàn tất.")) return;

        const logEl = document.getElementById('uploadLog');
        logEl.textContent = "Đang tải file .bin lên ESP32-C3...";

        let formData = new FormData();
        formData.append("update", selectedBinFile);

        try {
            let res = await fetch("/update", {
                method: "POST",
                body: formData
            });
            let resultText = await res.text();
            logEl.textContent = "Kết quả OTA: " + resultText;
            if (res.ok) {
                alert("Nạp firmware thành công! Thiết bị đang khởi động lại.");
            }
        } catch(e) {
            logEl.textContent = "Lỗi kết nối trong quá trình nạp OTA: " + e;
        }
    }

    async function exitOtaMode() {
        if (confirm("Thoát chế độ OTA và quay lại chế độ thực hành?")) {
            await fetch('/exit-ota');
            alert("Đang khởi động lại thiết bị...");
        }
    }
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Mặc định khởi động ở PRACTICE MODE (Không bật Wi-Fi, không chạy web server)
  // Kiểm tra nhanh xem người dùng có giữ nút BOOT khi khởi động nguồn không
  unsigned long startCheck = millis();
  while (millis() - startCheck < 500) {
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      buttonPressStartTime = millis();
      buttonWasPressed = true;
      break;
    }
  }
}

void loop() {
  // Xử lý logic Dual-Mode bằng nút BOOT (GPIO 9)
  int buttonState = digitalRead(BOOT_BUTTON_PIN);

  if (buttonState == LOW) {
    if (!buttonWasPressed) {
      buttonWasPressed = true;
      buttonPressStartTime = millis();
    } else {
      // Nếu giữ nút liên tục hơn 3 giây và đang ở chế độ PRACTICE -> Chuyển sang OTA_READY MODE
      if (systemMode == 0 && (millis() - buttonPressStartTime >= 3000)) {
        systemMode = 1; // Chuyển sang OTA_READY
        Serial.println("[DUAL-MODE] Đã giữ BOOT > 3s. Khởi động Wi-Fi AP & Web IDE Server...");
        
        WiFi.softAP(ssid, password);
        IPAddress myIP = WiFi.softAPIP();
        Serial.print("AP IP Address: ");
        Serial.println(myIP);

        // Khởi động Web Server endpoint
        server.on("/", HTTP_GET, []() {
          server.send_P(200, "text/html", MAIN_page);
        });

        server.on("/exit-ota", HTTP_GET, []() {
          server.send(200, "text/plain", "Exiting OTA mode...");
          delay(500);
          ESP.restart();
        });

        // Endpoint tiếp nhận cập nhật OTA chuẩn core 3.x sử dụng Update.h
        server.on("/update", HTTP_POST, []() {
          server.send(200, "text/plain", Update.hasError() ? "FAIL" : "SUCCESS");
          delay(1000);
          ESP.restart();
        }, []() {
          HTTPUpload& upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("Bắt đầu cập nhật OTA: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
              Serial.printf("Cập nhật OTA thành công: %u bytes\n", upload.totalSize);
            } else {
              Update.printError(Serial);
            }
          }
        });

        server.begin();
      }
    }
  } else {
    buttonWasPressed = false;
  }

  // Thực thi tác vụ tương ứng với chế độ hiện tại
  if (systemMode == 0) {
    // === CHẾ ĐỘ THỰC HÀNH (PRACTICE MODE) ===
    // Chạy chương trình ứng dụng thực tế (Ví dụ nháy LED chân 8)
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH);
    delay(500);
    digitalWrite(8, LOW);
    delay(500);
  } else if (systemMode == 1) {
    // === CHẾ ĐỘ OTA READY ===
    server.handleClient();
  }
}