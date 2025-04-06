#include <napi.h>
#include <iostream>
#include <map>
#include <vector>
#include <string>

// Include YASDI SDK headers
extern "C" {
    #include "libyasdi.h"
    #include "libyasdimaster.h"
    #include "tools.h"
}

// Structure to store channel data
struct ChannelData {
    std::string name;
    std::string units;
    std::string value;
    double numericValue;
};

class InverterWrapper : public Napi::ObjectWrap<InverterWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    InverterWrapper(const Napi::CallbackInfo& info);
    ~InverterWrapper();

private:
    static Napi::FunctionReference constructor;
    
    // YASDI SDK methods wrapped for Node.js
    Napi::Value Initialize(const Napi::CallbackInfo& info);
    Napi::Value DetectDevices(const Napi::CallbackInfo& info);
    Napi::Value GetDevices(const Napi::CallbackInfo& info);
    Napi::Value GetDeviceData(const Napi::CallbackInfo& info);
    Napi::Value SetChannelValue(const Napi::CallbackInfo& info);
    Napi::Value GetChannelInfo(const Napi::CallbackInfo& info);
    Napi::Value Shutdown(const Napi::CallbackInfo& info);
    
    // Internal helper methods
    bool detect_devices(int device_count);
    std::map<DWORD, std::string> get_device_map();
    std::vector<ChannelData> fetch_channel_data(DWORD device_handle);
    DWORD find_channel_handle(DWORD device_handle, const std::string& channel_name);
    
    // Member variables
    bool initialized = false;
    DWORD drivers[10]; // Assuming max 10 drivers
    DWORD driver_count = 0;
    int debug_level = 0;
};

Napi::FunctionReference InverterWrapper::constructor;

Napi::Object InverterWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);
    
    Napi::Function func = DefineClass(env, "InverterWrapper", {
        InstanceMethod("initialize", &InverterWrapper::Initialize),
        InstanceMethod("detectDevices", &InverterWrapper::DetectDevices),
        InstanceMethod("getDevices", &InverterWrapper::GetDevices),
        InstanceMethod("getDeviceData", &InverterWrapper::GetDeviceData),
        InstanceMethod("setChannelValue", &InverterWrapper::SetChannelValue),
        InstanceMethod("getChannelInfo", &InverterWrapper::GetChannelInfo),
        InstanceMethod("shutdown", &InverterWrapper::Shutdown)
    });
    
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    
    exports.Set("InverterWrapper", func);
    return exports;
}

InverterWrapper::InverterWrapper(const Napi::CallbackInfo& info) 
    : Napi::ObjectWrap<InverterWrapper>(info) {
    
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);
    
    if (info.Length() > 0 && info[0].IsNumber()) {
        this->debug_level = info[0].As<Napi::Number>().Int32Value();
    }
}

InverterWrapper::~InverterWrapper() {
    if (initialized) {
        // Shutdown YASDI if still initialized
        for (DWORD i = 0; i < driver_count; i++) {
            yasdiSetDriverOffline(drivers[i]);
        }
        yasdiMasterShutdown();
    }
}

Napi::Value InverterWrapper::Initialize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Config file path expected as argument").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    std::string config_path = info[0].As<Napi::String>().Utf8Value();
    // Initialize YASDI library
    DWORD result = yasdiMasterInitialize(config_path.c_str(), &driver_count);
    
    if (this->debug_level > 0) {
        std::cout << "YASDI initialization returned: " << result << std::endl;
        std::cout << "Found " << driver_count << " drivers" << std::endl;
    }
    
    if (driver_count == 0) {
        Napi::Error::New(env, "No YASDI drivers found").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
    
    // Get all drivers
    driver_count = yasdiMasterGetDriver(drivers, 10);
    
    // Switch all drivers online
    bool any_driver_online = false;
    for (DWORD i = 0; i < driver_count; i++) {
        char driverName[64];
        yasdiGetDriverName(drivers[i], driverName, sizeof(driverName) - 1);
        
        if (this->debug_level > 0) {
            std::cout << "Switching on driver: " << driverName << std::endl;
        }
        
        if (yasdiSetDriverOnline(drivers[i])) {
            any_driver_online = true;
        }
    }
    
    if (!any_driver_online) {
        Napi::Error::New(env, "No drivers could be set online").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
    
    this->initialized = true;
    return Napi::Boolean::New(env, true);
}

Napi::Value InverterWrapper::DetectDevices(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!initialized) {
        Napi::Error::New(env, "YASDI not initialized. Call initialize() first").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    int device_count = 1; // Default to 1 device
    if (info.Length() > 0 && info[0].IsNumber()) {
        device_count = info[0].As<Napi::Number>().Int32Value();
    }
    
    bool success = detect_devices(device_count);
    return Napi::Boolean::New(env, success);
}

bool InverterWrapper::detect_devices(int device_count) {
    if (this->debug_level > 0) {
        std::cout << "Trying to detect " << device_count << " devices" << std::endl;
    }
    
    // Blocking call to detect devices
    int error = DoStartDeviceDetection(device_count, TRUE);
    
    switch (error) {
        case YE_OK:
            return true;
        case YE_DEV_DETECT_IN_PROGRESS:
            std::cout << "Error: Detection in progress" << std::endl;
            return false;
        case YE_NOT_ALL_DEVS_FOUND:
            std::cout << "Error: Not all devices were found" << std::endl;
            return false;
        default:
            std::cout << "Error: Unknown YASDI error" << std::endl;
            return false;
    }
}

Napi::Value InverterWrapper::GetDevices(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!initialized) {
        Napi::Error::New(env, "YASDI not initialized. Call initialize() first").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    std::map<DWORD, std::string> device_map = get_device_map();
    
    Napi::Array devices = Napi::Array::New(env);
    int i = 0;
    
    for (const auto& device : device_map) {
        Napi::Object dev = Napi::Object::New(env);
        dev.Set("handle", Napi::Number::New(env, device.first));
        dev.Set("name", Napi::String::New(env, device.second));
        devices[i++] = dev;
    }
    
    return devices;
}

std::map<DWORD, std::string> InverterWrapper::get_device_map() {
    DWORD handles_array[50];
    char namebuf[64] = "";
    std::map<DWORD, std::string> device_map;
    
    // Get all device handles
    DWORD count = GetDeviceHandles(handles_array, 50);
    
    if (count > 0) {
        for (DWORD device = 0; device < count; device++) {
            // Get the name of this device
            GetDeviceName(handles_array[device], namebuf, sizeof(namebuf) - 1);
            
            if (this->debug_level > 0) {
                std::cout << "Found device with a handle of: " << handles_array[device] 
                          << " and a name of: " << namebuf << std::endl;
            }
            
            std::string device_name = std::string(namebuf);
            // Replace spaces with underscores for easier handling
            size_t pos;
            while ((pos = device_name.find(" ")) != std::string::npos) {
                device_name.replace(pos, 1, "_");
            }
            
            // Add it to the map
            device_map[handles_array[device]] = device_name;
        }
    } else {
        if (this->debug_level > 0) {
            std::cout << "No devices have been found" << std::endl;
        }
    }
    
    return device_map;
}

Napi::Value InverterWrapper::GetDeviceData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!initialized) {
        Napi::Error::New(env, "YASDI not initialized. Call initialize() first").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    if (info.Length() < 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "Device handle expected as argument").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    DWORD device_handle = info[0].As<Napi::Number>().Int32Value();
    std::vector<ChannelData> channel_data = fetch_channel_data(device_handle);
    
    Napi::Object result = Napi::Object::New(env);
    result.Set("timestamp", Napi::String::New(env, ""));  // We'll set this in JS
    
    for (const auto& data : channel_data) {
        Napi::Object channelObj = Napi::Object::New(env);
        channelObj.Set("value", Napi::String::New(env, data.value));
        channelObj.Set("units", Napi::String::New(env, data.units));
        channelObj.Set("numericValue", Napi::Number::New(env, data.numericValue));
        
        result.Set(data.name, channelObj);
    }
    
    return result;
}

std::vector<ChannelData> InverterWrapper::fetch_channel_data(DWORD device_handle) {
    DWORD channel_array[500];
    char channel_name[64];
    char channel_units[64];
    char channel_value[64];
    double double_val = 0;
    std::vector<ChannelData> data_vector;
    DWORD max_age = 5;  // Maximum age of the channel value in seconds
    
    int channel_count = GetChannelHandlesEx(device_handle, channel_array, 500, SPOTCHANNELS);
    
    if (channel_count < 1) {
        if (this->debug_level > 0) {
            std::cout << "Could not get the channel count" << std::endl;
        }
        return data_vector;
    }
    
    // Process each channel
    for (int i = 0; i < channel_count; i++) {
        int result = GetChannelName(channel_array[i], channel_name, sizeof(channel_name) - 1);
        
        if (result != YE_OK) {
            if (this->debug_level > 0) {
                std::cout << "Error reading channel name" << std::endl;
            }
            continue;
        }
        
        // Get the units of the reading (e.g., kWh, V, etc.)
        GetChannelUnit(channel_array[i], channel_units, sizeof(channel_units) - 1);
        
        // Get the channel value
        result = GetChannelValue(channel_array[i], device_handle, &double_val, channel_value, sizeof(channel_value) - 1, max_age);
        
        if (result != YE_OK) {
            if (this->debug_level > 0) {
                std::cout << "Error reading channel value for channel: " << channel_name << std::endl;
            }
            continue;
        }
        
        // Store the data
        ChannelData data;
        data.name = channel_name;
        data.units = channel_units;
        data.value = channel_value;
        data.numericValue = double_val;
        
        data_vector.push_back(data);
    }
    
    return data_vector;
}

// Helper function to find a channel handle by name
DWORD InverterWrapper::find_channel_handle(DWORD device_handle, const std::string& channel_name) {
    DWORD channel_array[500];
    char name_buffer[64];
    
    int channel_count = GetChannelHandlesEx(device_handle, channel_array, 500, ALLCHANNELS);
    
    if (channel_count < 1) {
        if (this->debug_level > 0) {
            std::cout << "Could not get channel handles" << std::endl;
        }
        return 0;
    }
    
    for (int i = 0; i < channel_count; i++) {
        int result = GetChannelName(channel_array[i], name_buffer, sizeof(name_buffer) - 1);
        
        if (result == YE_OK && channel_name == name_buffer) {
            return channel_array[i];
        }
    }
    
    return 0; // Channel not found
}

// New method to get channel information (including range)
Napi::Value InverterWrapper::GetChannelInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!initialized) {
        Napi::Error::New(env, "YASDI not initialized. Call initialize() first").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected arguments: deviceHandle (number), channelName (string)").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    DWORD device_handle = info[0].As<Napi::Number>().Int32Value();
    std::string channel_name = info[1].As<Napi::String>().Utf8Value();
    
    DWORD channel_handle = find_channel_handle(device_handle, channel_name);
    
    if (channel_handle == 0) {
        Napi::Error::New(env, "Channel not found").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    double min_value, max_value;
    int result = GetChannelValRange(channel_handle, &min_value, &max_value);
    
    if (result != YE_OK) {
        if (this->debug_level > 0) {
            std::cout << "Error getting channel value range: " << result << std::endl;
        }
        
        Napi::Error::New(env, "Failed to get channel value range").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    char channel_units[64];
    GetChannelUnit(channel_handle, channel_units, sizeof(channel_units) - 1);
    
    Napi::Object channelInfo = Napi::Object::New(env);
    channelInfo.Set("handle", Napi::Number::New(env, channel_handle));
    channelInfo.Set("name", Napi::String::New(env, channel_name));
    channelInfo.Set("minValue", Napi::Number::New(env, min_value));
    channelInfo.Set("maxValue", Napi::Number::New(env, max_value));
    channelInfo.Set("units", Napi::String::New(env, channel_units));
    
    return channelInfo;
}

// New method to set channel values
Napi::Value InverterWrapper::SetChannelValue(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!initialized) {
        Napi::Error::New(env, "YASDI not initialized. Call initialize() first").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    // Check arguments
    if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsString() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: deviceHandle (number), channelName (string), value (number)").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    DWORD device_handle = info[0].As<Napi::Number>().Int32Value();
    std::string channel_name = info[1].As<Napi::String>().Utf8Value();
    double value = info[2].As<Napi::Number>().DoubleValue();
    
    // Find the channel handle by name
    DWORD channel_handle = find_channel_handle(device_handle, channel_name);
    
    if (channel_handle == 0) {
        Napi::Error::New(env, "Channel not found").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    // Check if value is within valid range
    double min_value, max_value;
    int range_result = GetChannelValRange(channel_handle, &min_value, &max_value);
    
    if (range_result == YE_OK && (value < min_value || value > max_value)) {
        if (this->debug_level > 0) {
            std::cout << "Value out of range. Valid range: [" << min_value << ", " << max_value << "]" << std::endl;
        }
        
        Napi::Object result = Napi::Object::New(env);
        result.Set("success", Napi::Boolean::New(env, false));
        result.Set("error", Napi::String::New(env, "Value out of range"));
        result.Set("code", Napi::Number::New(env, YE_VALUE_NOT_VALID));
        result.Set("validRange", Napi::Object::New(env));
        result.Get("validRange").As<Napi::Object>().Set("min", Napi::Number::New(env, min_value));
        result.Get("validRange").As<Napi::Object>().Set("max", Napi::Number::New(env, max_value));
        return result;
    }
    
    // Set the channel value
    int set_result = ::SetChannelValue(channel_handle, device_handle, value);
    
    Napi::Object result = Napi::Object::New(env);
    result.Set("success", Napi::Boolean::New(env, set_result == YE_OK));
    result.Set("code", Napi::Number::New(env, set_result));
    
    // Handle error cases
    if (set_result != YE_OK) {
        std::string error_message;
        
        switch (set_result) {
            case INVALID_HANDLE:
                error_message = "Invalid channel handle";
                break;
            case YE_SHUTDOWN:
                error_message = "YASDI is in shutdown mode";
                break;
            case YE_TIMEOUT:
                error_message = "Device did not respond (timeout)";
                break;
            case YE_VALUE_NOT_VALID:
                error_message = "Channel value not within valid range";
                break;
            case YE_NO_ACCESS_RIGHTS:
                error_message = "Not enough access rights to write to channel";
                break;
            default:
                error_message = "Unknown error";
                break;
        }
        
        result.Set("error", Napi::String::New(env, error_message));
        
        if (this->debug_level > 0) {
            std::cout << "Error setting channel value: " << error_message << " (code: " << set_result << ")" << std::endl;
        }
    }
    
    return result;
}

Napi::Value InverterWrapper::Shutdown(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!initialized) {
        return Napi::Boolean::New(env, true);
    }
    
    // Shutdown all YASDI drivers
    for (DWORD i = 0; i < driver_count; i++) {
        yasdiSetDriverOffline(drivers[i]);
    }
    
    // Shutdown YASDI
    yasdiMasterShutdown();
    
    initialized = false;
    return Napi::Boolean::New(env, true);
}

// Initialize the native addon
Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    return InverterWrapper::Init(env, exports);
}

NODE_API_MODULE(inverter_sdk, InitAll)