# Linux_Explorer (TermScope)

A lightweight terminal-based resource monitor for Linux.

TermScope displays real-time information about CPU usage (per-core), memory, swap, load average, network throughput, and running processes. It features a simple custom TUI with sorting, searching, and basic interactive controls.

> Note: Calling it a “resource manager” is a bit of a stretch — it’s primarily a monitor.

## Features

- Per-core CPU usage and frequency
- Memory and swap usage
- System load average
- Network RX/TX rates
- Process list with sorting (by CPU or memory) and search
- Lightweight custom terminal UI (no ncurses dependency)
- Color support (can be disabled)
- One-shot mode for scripting

## Requirements

- A C11-compatible compiler (`gcc` or `clang`)
- Linux (relies on `/proc` filesystem)
- Standard POSIX libraries

## Building

```bash
make
```

This will produce the binary `tscope`.

### Install (optional)

```bash
sudo make install
```

By default it installs to `/usr/bin/tscope`. You can change the prefix:

```bash
make install PREFIX=/usr/local
```

### Uninstall

```bash
sudo make uninstall
```

## Usage

```bash
./tscope [options]
```

### Options

| Option          | Description                              |
|-----------------|------------------------------------------|
| `--interval SEC`| Sampling interval in seconds (default: 1.0) |
| `--no-color`    | Disable colors                           |
| `--once`        | Print one report and exit                |
| `--version`     | Show version                             |
| `--help`        | Show help                                |

### Interactive Controls

| Key       | Action                     |
|-----------|----------------------------|
| `q` / `Q` | Quit                       |
| `p` / `P` | Pause / resume             |
| `r` / `R` | Force refresh              |
| `c` / `C` | Sort by CPU                |
| `m` / `M` | Sort by memory             |
| `/`       | Search processes           |
| `↑` / `↓` | Select process             |
| `?`       | Toggle help                |
| `Ctrl+C`  | Quit                       |

## License

MIT License  
Copyright (c) 2026 特蕾莎

## Contributing

Pull requests and suggestions are welcome.  
If you notice any awkward English in the documentation, feel free to open an issue or PR — native speakers are especially appreciated.