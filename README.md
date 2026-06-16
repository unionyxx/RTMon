# RTMon (LatencyMon for Linux)

RTMon is a real-time system latency monitor for Linux, inspired by LatencyMon. It provides detailed insights into system scheduling latencies, interrupt handling, and page faults to help diagnose real-time performance issues, such as audio dropouts or gaming stutter.

## Features

- **Real-time Latency Tracking**: Monitor current and peak scheduling latencies in microseconds.
- **Interrupt Analysis**: View Hard IRQ and Soft IRQ counts per second, identified by source.
- **Page Fault Monitoring**: Track major page faults that can cause unexpected stalls.
- **Diagnostic Reports**: Automated analysis of system health with actionable advice.
- **System Tray Integration**: Minimizes to the tray for background monitoring.
- **Modern UI**: Built with Qt 6 and C++20, supporting Wayland natively.

## Requirements

- **Linux Kernel**: Tested on modern kernels.
- **Qt 6**: Core, Gui, and Widgets modules.
- **C++20 Compiler**: GCC or Clang with C++20 support.
- **CMake**: Version 3.16 or higher.

## Building

### Using CMake (Recommended)

```bash
mkdir build
cd build
cmake ..
make
./RTMon
```

### Using QMake

```bash
qmake RTMon.pro
make
./RTMon
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
