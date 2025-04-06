{
  "targets": [
    {
      "target_name": "inverter_sdk",
      "sources": [ "src/inverter_wrapper.cc" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/include",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/core",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/smalib",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/os",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/protocol",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/master",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/libs",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/projects/generic-cmake/incprj",
        "<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/projects/generic-cmake/build-gcc"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "libraries": [
        "-L<!(node -p \"process.env.YASDI_SDK_PATH || './yasdi/'\")/lib",
        "-lyasdi",
        "-lyasdimaster"
      ]
    }
  ]
}