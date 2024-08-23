#pragma once

#define MUTEX_NAME                  L"Global\\de.icarus.micmute"
#define SERVER_PROC_NAME            L"ControlServer.exe"

#define SUCCESSFUL                  0x00
#define ALREADY_RUNNING             0x01
#define COM_INIT_FAILED             0x02
#define SNAPSHOT_FAILED             0x03
#define PORT_FIND_FAILED            0x04
#define COCREATE_FAILED             0x05
#define WSA_START_FAILED            0x06
#define WSA_INVALID_SOCKET          0x07
#define WSA_SOCKET_ERROR            0x08

// TODO automatically retrieve devid and port on replugging
#define KEEPALIVE_MSG               "Length=00000D <keep-alive/>"
#define INIT_MSG                    "Length=000043 <client-details client-key=\"xxxxxxxx-0000-xxxx-xxxx-xxxxxxxxxxxx\"/>"
#define MODE_COLOR_MSG              "Length=000031 <set devid=\"1\"><item id=\"45\" value=\"true\"/></set>"
#define MODE_AUDIO_MSG              "Length=000032 <set devid=\"1\"><item id=\"45\" value=\"false\"/></set>"
#define COLOR_RED_MSG               "Length=000031 <set devid=\"1\"><item id=\"46\" value=\"pink\"/></set>"

#define FUNCTION_KEY                0x7C // F15

#define USE_SOUND
#define USE_FOCUSRITE
#define USE_CONSOLE
