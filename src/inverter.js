const { InverterWrapper } = require("../build/Release/inverter_sdk");

class SMInverter {
  constructor(options = {}) {
    this.configPath = options.configPath || "/home/user/yasdi.ini";
    this.debugLevel = options.debugLevel || 0;
    this.wrapper = new InverterWrapper(this.debugLevel);
    this.initialized = false;
    this.deviceMap = new Map();
  }

  /**
   * Initialize the YASDI SDK
   * @returns {Promise<boolean>} Success status
   */
  async initialize() {
    try {
      const result = this.wrapper.initialize(this.configPath);
      this.initialized = result;
      return result;
    } catch (error) {
      console.error("Failed to initialize YASDI:", error);
      return false;
    }
  }

  /**
   * Detect SMA inverter devices
   * @param {number} deviceCount Number of devices to look for
   * @returns {Promise<boolean>} Success status
   */
  async detectDevices(deviceCount = 1) {
    this._checkInitialized();
    try {
      return this.wrapper.detectDevices(deviceCount);
    } catch (error) {
      console.error("Failed to detect devices:", error);
      return false;
    }
  }

  /**
   * Get all detected inverter devices
   * @returns {Promise<Array>} Array of device objects
   */
  async getDevices() {
    this._checkInitialized();
    try {
      const devices = this.wrapper.getDevices();
      // Cache device map for easier lookup
      devices.forEach((device) => {
        this.deviceMap.set(device.name, device.handle);
      });
      return devices;
    } catch (error) {
      console.error("Failed to get devices:", error);
      return [];
    }
  }

  /**
   * Get live data from a specific inverter device
   * @param {number|string} deviceHandle Device handle or name
   * @returns {Promise<Object>} Inverter data
   */
  async getDeviceData(deviceHandle) {
    this._checkInitialized();

    // If a string is provided, look up the device handle
    if (typeof deviceHandle === "string") {
      if (this.deviceMap.has(deviceHandle)) {
        deviceHandle = this.deviceMap.get(deviceHandle);
      } else {
        // Try to refresh device map
        await this.getDevices();
        if (this.deviceMap.has(deviceHandle)) {
          deviceHandle = this.deviceMap.get(deviceHandle);
        } else {
          throw new Error(`Device with name "${deviceHandle}" not found`);
        }
      }
    }

    try {
      const data = this.wrapper.getDeviceData(deviceHandle);
      // Add timestamp
      data.timestamp = new Date().toISOString();
      return this._processData(data);
    } catch (error) {
      console.error(`Failed to get data for device ${deviceHandle}:`, error);
      return null;
    }
  }

  /**
   * Get information about a specific channel including valid value range
   * @param {number|string} deviceHandle Device handle or name
   * @param {string} channelName Name of the channel
   * @returns {Promise<Object>} Channel information including min/max values and units
   */
  async getChannelInfo(deviceHandle, channelName) {
    this._checkInitialized();

    // If a string is provided, look up the device handle
    if (typeof deviceHandle === "string") {
      deviceHandle = await this._resolveDeviceHandle(deviceHandle);
    }

    try {
      return this.wrapper.getChannelInfo(deviceHandle, channelName);
    } catch (error) {
      console.error(`Failed to get channel info for ${channelName}:`, error);
      return null;
    }
  }

  /**
   * Set a value for a specific channel on a device
   * @param {number|string} deviceHandle Device handle or name
   * @param {string} channelName Name of the channel to set
   * @param {number} value Value to set for the channel
   * @returns {Promise<Object>} Result of the operation
   */
  async setChannelValue(deviceHandle, channelName, value) {
    this._checkInitialized();

    // If a string is provided, look up the device handle
    if (typeof deviceHandle === "string") {
      deviceHandle = await this._resolveDeviceHandle(deviceHandle);
    }

    try {
      const result = this.wrapper.setChannelValue(
        deviceHandle,
        channelName,
        value
      );
      return result;
    } catch (error) {
      console.error(`Failed to set channel value for ${channelName}:`, error);
      return {
        success: false,
        error: error.message,
        code: -1,
      };
    }
  }

  /**
   * Helper method to resolve a device name to its handle
   * @param {string} deviceName Name of the device
   * @returns {Promise<number>} Device handle
   * @private
   */
  async _resolveDeviceHandle(deviceName) {
    if (this.deviceMap.has(deviceName)) {
      return this.deviceMap.get(deviceName);
    }

    // Try to refresh device map
    await this.getDevices();

    if (this.deviceMap.has(deviceName)) {
      return this.deviceMap.get(deviceName);
    }

    throw new Error(`Device with name "${deviceName}" not found`);
  }

  /**
   * Process raw inverter data into a more structured format
   * @param {Object} rawData Raw data from the inverter
   * @returns {Object} Processed data
   */
  _processData(rawData) {
    const result = {
      timestamp: rawData.timestamp,
      dc: {
        string1: {
          current: this._extractValue(rawData, [
            "A.Ms.Amp",
            "pv panels current",
          ]),
          voltage: this._extractValue(rawData, [
            "A.Ms.Vol",
            "pv input voltage",
          ]),
          power: this._extractValue(rawData, ["A.Ms.Watt"]),
        },
        string2: {
          current: this._extractValue(rawData, ["B.Ms.Amp"]),
          voltage: this._extractValue(rawData, ["B.Ms.Vol"]),
          power: this._extractValue(rawData, ["B.Ms.Watt"]),
        },
      },
      ac: {
        total: {
          power: this._extractValue(rawData, ["grid power"]),
        },
        phase1: {
          power: this._extractValue(rawData, ["GridMs.W.phsA"]),
          voltage: this._extractValue(rawData, [
            "GridMs.PhV.phsA",
            "grid voltage",
          ]),
          current: this._extractValue(rawData, [
            "GridMs.A.phsA",
            "current to grid",
          ]),
        },
        phase2: {
          power: this._extractValue(rawData, ["GridMs.W.phsB"]),
          voltage: this._extractValue(rawData, ["GridMs.PhV.phsB"]),
          current: this._extractValue(rawData, ["GridMs.A.phsB"]),
        },
        phase3: {
          power: this._extractValue(rawData, ["GridMs.W.phsC"]),
          voltage: this._extractValue(rawData, ["GridMs.PhV.phsC"]),
          current: this._extractValue(rawData, ["GridMs.A.phsC"]),
        },
        frequency: this._extractValue(rawData, ["GridMs.Hz", "grid freq"]),
        powerFactor: this._extractValue(rawData, ["GridMs.TotPF"]),
      },
      system: {
        status: this._extractValue(rawData, ["Mode", "Status"]),
        error: this._extractValue(rawData, ["Error", "error"]),
        operatingHours: this._extractValue(rawData, ["total operating hours"]),
        isolationResistance: this._extractValue(rawData, ["isol-resist"]),
        totalYield: this._extractValue(rawData, ["energy yield"]),
      },
      raw: rawData, // Include raw data for reference
    };

    return result;
  }

  /**
   * Extract a value from rawData given possible key names
   * @param {Object} rawData Raw data object
   * @param {Array<string>} possibleKeys Possible key names
   * @returns {Object|null} Value object or null if not found
   */
  _extractValue(rawData, possibleKeys) {
    for (const key of possibleKeys) {
      if (rawData[key]) {
        return rawData[key];
      }
    }
    return null;
  }

  /**
   * Shutdown the YASDI SDK
   * @returns {Promise<boolean>} Success status
   */
  async shutdown() {
    if (!this.initialized) return true;

    try {
      const result = this.wrapper.shutdown();
      this.initialized = !result;
      return result;
    } catch (error) {
      console.error("Failed to shutdown YASDI:", error);
      return false;
    }
  }

  /**
   * Check if the SDK is initialized
   * @private
   */
  _checkInitialized() {
    if (!this.initialized) {
      throw new Error("SDK not initialized. Call initialize() first");
    }
  }
}

module.exports = SMInverter;
