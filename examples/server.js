const express = require("express");
const http = require("http");
const socketIo = require("socket.io");
const path = require("path");
const SMInverter = require("../"); // Use require('sma-inverter-sdk') in real usage

// Create Express application
const app = express();
const server = http.createServer(app);
const io = socketIo(server);

// Serve static files
app.use(express.static(path.join(__dirname, "public")));
app.use(express.json());

// Initialize the inverter
const inverter = new SMInverter({
  configPath: process.env.YASDI_CONFIG || "/home/haythem/yasdi.ini",
  debugLevel: parseInt(process.env.DEBUG_LEVEL || "1", 10),
});

// Device cache
let devices = [];
let selectedDevice = null;
let pollingInterval = null;
const POLL_INTERVAL = 5000; // 5 seconds

// Initialize the inverter on startup
async function initInverter() {
  console.log("Initializing SMA Inverter SDK...");
  const initialized = await inverter.initialize();

  if (initialized) {
    console.log("SDK initialized successfully");
    console.log("Detecting devices...");

    const detected = await inverter.detectDevices();
    if (detected) {
      console.log("Device detection completed");
      devices = await inverter.getDevices();
      console.log(`Found ${devices.length} devices`);

      if (devices.length > 0) {
        selectedDevice = devices[0].handle;
        console.log(`Selected device: ${devices[0].name} (${selectedDevice})`);
      }
    } else {
      console.error("Device detection failed");
    }
  } else {
    console.error("Failed to initialize SDK");
  }
}

// API Routes
app.get("/api/devices", async (req, res) => {
  try {
    if (!inverter.initialized) {
      await initInverter();
    }

    devices = await inverter.getDevices();
    res.json(devices);
  } catch (error) {
    console.error("Error getting devices:", error);
    res.status(500).json({ error: "Failed to get devices" });
  }
});

app.get("/api/data", async (req, res) => {
  try {
    if (!selectedDevice) {
      return res.status(404).json({ error: "No device selected" });
    }

    const data = await inverter.getDeviceData(selectedDevice);
    res.json(data);
  } catch (error) {
    console.error("Error getting inverter data:", error);
    res.status(500).json({ error: "Failed to get inverter data" });
  }
});

app.post("/api/device/select", async (req, res) => {
  try {
    const { handle } = req.body;
    if (!handle) {
      return res.status(400).json({ error: "Device handle required" });
    }

    selectedDevice = handle;
    res.json({ success: true, selectedDevice });
  } catch (error) {
    console.error("Error selecting device:", error);
    res.status(500).json({ error: "Failed to select device" });
  }
});

// WebSocket for real-time updates
io.on("connection", (socket) => {
  console.log("Client connected");

  // Start polling if not already started
  if (!pollingInterval && selectedDevice) {
    startPolling();
  }

  socket.on("start-polling", () => {
    if (!pollingInterval && selectedDevice) {
      startPolling();
    }
  });

  socket.on("stop-polling", () => {
    stopPolling();
  });

  socket.on("disconnect", () => {
    console.log("Client disconnected");
    // If no clients connected, stop polling
    if (io.engine.clientsCount === 0) {
      stopPolling();
    }
  });
});

function startPolling() {
  console.log("Starting data polling");
  pollingInterval = setInterval(async () => {
    try {
      if (selectedDevice) {
        const data = await inverter.getDeviceData(selectedDevice);
        io.emit("inverter-data", data);
      }
    } catch (error) {
      console.error("Error polling inverter data:", error);
    }
  }, POLL_INTERVAL);
}

function stopPolling() {
  if (pollingInterval) {
    console.log("Stopping data polling");
    clearInterval(pollingInterval);
    pollingInterval = null;
  }
}

// Graceful shutdown
process.on("SIGINT", async () => {
  console.log("Shutting down...");
  stopPolling();
  await inverter.shutdown();
  process.exit(0);
});

// Start the server
const PORT = process.env.PORT || 3000;
server.listen(PORT, async () => {
  console.log(`Server running on port ${PORT}`);
  await initInverter();
});
