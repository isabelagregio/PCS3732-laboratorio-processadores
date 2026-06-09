<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Controle do LED</title>
  <style>
    body { font-family: Arial; text-align: center; margin-top: 50px; }
    input[type=range] { width: 80%; max-width: 400px; display: block; margin: 10px auto; }
  </style>
  <script>
    function updateLED() {
      const val = document.getElementById('ledRange').value;
      const freq = document.getElementById('freqInput').value;
      
      document.getElementById('ledVal').innerText = val;
      
      // Envia os dois parâmetros na URL
      fetch(`http://192.168.4.1/setLED?val=${val}&freq=${freq}`);
    }
  </script>
</head>
<body>
  <h2>Controle de LED (ESP32)</h2>
  
  <p>Brilho (Duty Cycle): <span id="ledVal">0</span></p>
  <input type="range" id="ledRange" min="0" max="255" value="0" oninput="updateLED()">
  
  <p>Frequência (Hz):</p>
  <input type="number" id="freqInput" value="5000" onchange="updateLED()">
</body>
</html>
