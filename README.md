# Vector Active for Pebble

A Pebble watchface modeled on the Pixel Watch Active face. It uses the Pebble Complications and Plugins API (`preferences/complications` and `Pebble.enumeratePlugins()`) to fill three ring slots and two circular badge slots.

## Features

- **Complications integration.** Reads your ordered `preferences/complications` list from the Pebble app, or lets you pick any installed plugin source, property, and shape per slot.
- **Automatic slot hiding.** Hides any of the five slots that have no complication assigned, rather than showing placeholder icons.
- **Eight color themes.** Supports Campari, Sand, Ash, Sage, Limoncello, Sky, Periwinkle, and Flamingo with live updates from the settings page.
- **Font and display options.** Lets you switch the time font between LECO and Gothic, toggle the ring progress dot, and toggle leading zeros.

## Platform support

The watchface targets round color Pebble watches first, with dedicated layouts for Pebble Round 2 (`gabbro`, 260x260) and Pebble Time Round (`chalk`, 180x180).

It also includes secondary rectangular layouts for color square watches, supporting Pebble Time 2 (`emery`, 200x228) and Pebble Time and Pebble Time Steel (`basalt`, 144x168). I added basalt support for my Time Steel but I wouldn't recommend using it with complications more than a couple characters long.

Monochrome watches (`aplite`, `diorite`, and `flint`) are excluded for now, though monochrome support may be added later.

## Setup and configuration

1. Enable Plugins in the Pebble mobile app under Settings, Plugins, Plugins / permissions preview.
2. Add up to five readings under Settings, Plugins, Complications.
3. Install Pixel Active onto your watch.
4. Open the watchface settings page in the Pebble app to choose a color theme, time font, display toggles, or per-slot plugin overrides.

## Building from source

### Prerequisites

- Python 3.10 to 3.13
- Node.js and npm
- [`uv`](https://astral.sh/uv) (recommended)

### Install Pebble tool and SDK

```bash
# Install pebble-tool via uv
uv tool install pebble-tool --python 3.13

# Install the latest Pebble SDK
pebble sdk install latest
```

### Build and install

```bash
# Build the .pbw package
pebble build

# Install onto watch locally or through CloudPebble
pebble install --phone <PHONE_IP>
# Or
pebble install --cloudpebble

# Or run in the emulator
pebble install --emulator gabbro
pebble install --emulator chalk
pebble install --emulator emery
pebble install --emulator basalt
```

## License

MIT
