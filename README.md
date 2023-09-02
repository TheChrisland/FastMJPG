<img src="./logo.png" />

# FastMJPG (README)

**FastMJPG is a command line tool for capturing, sending, receiving, rendering, piping, and recording MJPG video with extremely low latency. It is optimized for running on constrained hardware and battery powered devices.**

It is written entirely in C, and leverages UDP for network transport, v4l2 for video capture, libturbojpeg for JPEG decoding, libglfw for OpenGL rendering, and ffmpeg for Matroska video packing.

It can be integrated directly into your C application as a library, piped to your application via a file descriptor, or used exclusively as a command line tool.

## Alpha

**FastMJPG is currently in a public alpha state. It is feature complete, and all known bugs have been fixed, though more issues are expected to be discovered as it is used by more people. It is not recommended for use in critical production environments at this time.**

## Install

```sh
# Install dependencies:
sudo apt-get update
sudo apt-get install git build-essential libturbojpeg libturbojpeg0-dev libglfw3-dev v4l-utils libavutil-dev libavcodec-dev libavformat-dev libswscale-dev

# Clone:
git clone git@github.com:adrianseeley/FastMJPG.git
cd ./FastMJPG

# Build:
chmod +x ./build
./build
cd ./bin

# Display help:
./FastMJPG help

# List compatible devices, resolutions, and framerates:
./FastMJPG devices

# Run FastMJPG:
./FastMJPG [input] [output 0] [output 1] ... [output n]
```

## Syntax

Exactly one input is required, either:

+ `capture` from a video capture device.
+ `receive` from a from another FastMJPG process over a network.

At least one output is also required, one or more of:

+ `render` to the screen using an OpenGL window.
+ `record` to a Matroska file as an MJPG stream.
+ `send` to another FastMJPG process over a network.
+ `pipe` to a file descriptor that your application provides.

## Capture (Input)

```sh
fj capture DEVICE_NAME RESOLUTION_WIDTH RESOLUTION_HEIGHT TIMEBASE_NUMERATOR TIMEBASE_DENOMINATOR ...
```

| Position | Argument | Type | Example | Description |
| :---: | --- | --- | --- | --- |
| 0 | DEVICE_NAME | string | `/dev/video0` | The path to the video device. |
| 1 | RESOLUTION_WIDTH | uint | `1280` | The width of the video stream. |
| 2 | RESOLUTION_HEIGHT | uint | `720` | The height of the video stream. |
| 3 | TIMEBASE_NUMERATOR | uint | `1` | The numerator of the framerate timebase. |
| 4 | TIMEBASE_DENOMINATOR | uint | `30` | The denominator of the framerate timebase. |

#### Notes

1. You must provide a resolution and framerate that is supported by the video device in MJPG streaming capture mode using MMAP buffers. FastMJPG will crash if the requested configuration is unsupported.
2. You can use `FastMJPG devices` to list all compatible devices, resolutions, and framerates.

## Receive (Input)

```sh
fj receive LOCAL_IP_ADDRESS LOCAL_PORT MAX_PACKET_LENGTH MAX_JPEG_LENGTH RESOLUTION_WIDTH RESOLUTION_HEIGHT TIMEBASE_NUMERATOR TIMEBASE_DENOMINATOR ...
```

| Position | Argument | Type | Example | Description |
| :---: | --- | --- | --- | --- |
| 0 | LOCAL_IP_ADDRESS | string | `127.0.0.1` | The IP address to listen on. |
| 1 | LOCAL_PORT | uint | `8000` | The port to listen on. |
| 2 | MAX_PACKET_LENGTH | uint | `1400` | The maximum length of an application layer packet in bytes. |
| 3 | MAX_JPEG_LENGTH | uint | `1000000` | The maximum length of a JPEG frame in bytes. |
| 4 | RESOLUTION_WIDTH | uint | `1280` | The width of the video stream. |
| 5 | RESOLUTION_HEIGHT | uint | `720` | The height of the video stream. |
| 6 | TIMEBASE_NUMERATOR | uint | `1` | The numerator of the framerate timebase. |
| 7 | TIMEBASE_DENOMINATOR | uint | `30` | The denominator of the framerate timebase. |

1. FastMJPG only supports receiving from other FastMJPG processes, it will not work with other software as it uses a custom application layer UDP protocol.
2. All stream configuration settings must exactly match that of the sender, otherwise it will result in undefined behaviour.
3. `MAX_PACKET_LENGTH` should be the largest value that fits into your network's MTU minus the overhead of the UDP header and IP header. MTU fragmented packets will result in undefined behaviour.
4. `MAX_JPEG_LENGTH` must be larger than the maximum JPEG frame size produced by the `capture`, otherwise it will result in undefined behaviour.

## Render (Output)

```sh
fj ... render WINDOW_WIDTH WINDOW_HEIGHT ...
```

| Position | Argument | Type | Example | Description |
| :---: | --- | --- | --- | --- |
| 0 | WINDOW_WIDTH | uint | `1280` | The width of the window. |
| 1 | WINDOW_HEIGHT | uint | `720` | The height of the window. |

1. Video will be rescaled to fit the window size on the GPU incurring little additional overhead. This rescaling will not affect any other outputs.
2. OpenGL windows are free-timed with the video stream, meaning the window may be declared unresponsive by the operating system if the video stream is paused or interrupted for any reason, this is normal behaiour and can safely be ignored.
3. OpenGL windows will not respond to the 'X' button to close for safety reasons, instead you must use `CTRL+C` to close the process with a `sigint` signal.
4. There can be at most one OpenGL window per process, attempting to create more than one will crash.

## Record (Output)

```sh
fj ... record FILE_NAME ...
```

| Position | Argument | Type | Example | Description |
| :---: | --- | --- | --- | --- |
| 0 | FILE_NAME | string | `/home/user/video.mkv` | The path to the output file. |

1. The `sigint` signal (`CTRL+C`) will cause FastMJPG to attempt to write trailers to the `mkv` file before shutting down, though the nature of MJPG streams in a Matroska container does not require this finalization for playback, allowing you to safely kill the process at any time.
2. If there are pauses in the video stream, the last frame before the pause will last for the duration of the pause, this is expected behaviour.

## Send (Output)

```sh
fj ... send LOCAL_IP_ADDRESS LOCAL_PORT REMOTE_IP_ADDRESS REMOTE_PORT MAX_PACKET_LENGTH MAX_JPEG_LENGTH SEND_ROUNDS ...
```

| Position | Argument | Type | Example | Description |
| :---: | --- | --- | --- | --- |
| 0 | LOCAL_IP_ADDRESS | string | `127.0.0.1` | The IP address to send from. |
| 1 | LOCAL_PORT | uint | `8000` | The port to send from. |
| 2 | REMOTE_IP_ADDRESS | string | `127.0.0.1` | The IP address to send to. |
| 3 | REMOTE_PORT | uint | `8000` | The port to send to. |
| 4 | MAX_PACKET_LENGTH | uint | `1400` | The maximum length of an application layer packet in bytes. |
| 5 | MAX_JPEG_LENGTH | uint | `1000000` | The maximum length of a JPEG frame in bytes. |
| 6 | SEND_ROUNDS | uint | `1` | The number of times to send each frame consecutively as a packet loss circumvention. |

1. FastMJPG only supports sending to other FastMJPG processes, it will not work with other software as it uses a custom application layer UDP protocol.
2. All stream configuration settings must exactly match that of the receiver, otherwise it will result in undefined behaviour.
3. `MAX_PACKET_LENGTH` should be the largest value that fits into your network's MTU minus the overhead of the UDP header and IP header. MTU fragmented packets will result in undefined behaviour.
4. `MAX_JPEG_LENGTH` must be larger than the maximum JPEG frame size produced by the `capture`, otherwise it will result in undefined behaviour.

## Pipe (Output)

```sh
fj ... pipe PIPE_FILE_DESCRIPTOR RGB_OR_JPEG MAX_PACKET_LENGTH ...
```

| Position | Argument | Type | Example | Description |
| :---: | --- | --- | --- | --- |
| 0 | PIPE_FILE_DESCRIPTOR | int | `3` | The open and initialized file descriptor of the pipe to write to. |
| 1 | RGB_OR_JPEG | string | `rgb` or `jpeg` | The format of the frames written to the pipe. |
| 2 | MAX_PACKET_LENGTH | uint | `4096` | The maximum length of a single write to the pipe. |

1. You must manage the pipe yourself, ensure that is it open and ready to write to before starting FastMJPG, and ensure that it is closed after FastMJPG has exited.
2. The pipe protocol is as follows.
```txt
Per Frame (RGB or JPEG):
1. big endian uint64_t uSeconds (capture timestamp)
2. big endian uint32_t length (frame length in bytes)
3. uint8_t[length] data (frame data)
```
3. RGB data is provided as three unsigned bytes per pixel, in the order of red, green, blue, with no padding and no alpha channel.
```c
// A 2x2 pixel RGB frame of all red would be 12 bytes long, 3 bytes per pixel, 4 pixels total, 2 pixels wide, 2 pixels tall and produce the following byte stream:
0xFF 0x00 0x00 0xFF 0x00 0x00 0xFF 0x00 0x00 0xFF 0x00 0x00
```
4. JPEG data does not contain MJPG frame separators, and is provided instead as a single properly formed JPEG.

## Author

All code and documentation was written by Adrian Seeley, who can be reached by email: [FastMJPG@adrianseeley.com](mailto:FastMJPG@adrianseeley.com)