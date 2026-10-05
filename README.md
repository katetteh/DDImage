# DDImage

A small, readable C++ image processing library for building computer vision projects, with the long-term goal of running on **embedded systems** (Arduino and ESP32 first, then STM32, M5Stack and others).

DDImage is written from scratch on top of a simple `Matrix` type. There is no heavy dependency chain: image decoding and encoding come from the bundled single-header [stb](https://github.com/nothings/stb) libraries, so the code is easy to read, easy to modify, and easy to fork into your own project.

> **Status:** early development (v0.1.0). The desktop library and the Qt demo work. The embedded work is in progress, see the [Roadmap](#roadmap).

---

## Table of contents

- [Features](#features)
- [Repository layout](#repository-layout)
- [Getting started](#getting-started)
- [Usage](#usage)
- [API reference](#api-reference)
- [Examples](#examples)
- [Known limitations](#known-limitations)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License and credits](#license-and-credits)

---

## Features

Every image function works on both **grayscale** (`Matrix`) and **color** (`RGBImage`) images.

| Area | Functions |
| --- | --- |
| Load / save | `load`, `load_rgb`, `save` |
| Resizing | `resize` with nearest-neighbor, bilinear, bicubic, and area interpolation |
| Filtering | `box_blur` (5x5), `Gaussian_blur` (5x5), `bilateralFilter` |
| Point operations | `negative` |
| Regions of interest | `ROI` (rectangular), `ROI_circular` (experimental) |
| Matrix utilities | transpose, add, subtract, search, display |

---

## Repository layout

```
DDImage/
├── CMakeLists.txt
├── include/ddimage/
│   ├── matrix.h              # Matrix class
│   ├── ImageProcessor.h      # RGBImage, InterpolationMethod, ImageProcessor
│   ├── stb_image.h           # image decoding (third party)
│   └── stb_image_write.h     # image encoding (third party)
├── src/
│   ├── matrix.cpp
│   └── ImageProcessor.cpp    # all image algorithms
├── examples/
│   ├── data/                 # sample images
│   ├── Image_processing_in_qt_window/   # desktop Qt viewer
│   └── eps32-CAM_webserver/             # ESP32-CAM live viewer (experimental)
└── tests/                    # not populated yet
```

---

## Getting started

### Requirements

- A C++17 compiler (GCC, Clang, or MSVC)
- CMake 3.16 or newer
- Qt 6 (Widgets, Multimedia, MultimediaWidgets), **only** if you want to build the Qt demo

### Build the library

```bash
git clone https://github.com/katetteh/DDImage.git
cd DDImage
cmake -S . -B build -DDDIMAGE_BUILD_EXAMPLES=OFF
cmake --build build
```

This produces the static library `libddimage.a` (`ddimage.lib` on Windows) in `build/`.

> `DDIMAGE_BUILD_EXAMPLES` is `ON` by default and requires Qt 6. If you do not have Qt installed, pass `-DDDIMAGE_BUILD_EXAMPLES=OFF` as shown above.

### Use it in your own CMake project

Place DDImage inside your project (as a git submodule or a plain copy) and link against it:

```cmake
add_subdirectory(DDImage)
target_link_libraries(my_app PRIVATE ddimage)
```

The `ddimage` target exposes its `include/` folder automatically, so this works right away:

```cpp
#include <ddimage/ImageProcessor.h>
```

---

## Usage

### Color image: load, resize, blur, save

```cpp
#include <ddimage/ImageProcessor.h>

int main() {
    ImageProcessor ip;
    int width, height;

    RGBImage img     = ip.load_rgb("photo.jpg", width, height);
    RGBImage half    = ip.resize(img, width / 2, height / 2, INTER_AREA);
    RGBImage blurred = ip.Gaussian_blur(half);

    ip.save("result.png", blurred);
    return 0;
}
```

### Grayscale image

```cpp
ImageProcessor ip;
int width, height;

Matrix gray     = ip.load("photo.jpg", width, height);   // converted to grayscale on load
Matrix inverted = ip.negative(gray);

ip.save("inverted.png", inverted, width, height);
```

### Working with `Matrix` directly

```cpp
Matrix m(2, 3);          // 2 rows, 3 columns
m[0][1] = 5;             // element access
m.insert(9, 1, 2);       // value, row, column
m.display();

Matrix t = m.transpose();   // 3 x 2
```

### Regions of interest

```cpp
// x, y, width, height
RGBImage crop = ip.ROI(img, 200, 200, 300, 300);
```

> **Note:** the processing functions take their image argument by non-const reference, so pass a named variable rather than a temporary. Chain calls with intermediate variables as in the examples above.

---

## API reference

### `Matrix` (`ddimage/matrix.h`)

A 2D grid of `int` values, stored as rows and columns. Used for grayscale images and for each color channel.

| Member | Description |
| --- | --- |
| `Matrix(int rows, int cols)` | Create a zero-filled matrix |
| `vector<int>& operator[](int row)` | Row access, so `m[r][c]` reads or writes an element |
| `int numRows() const` / `int numCols() const` | Dimensions |
| `bool isEmpty()` | True if there are no rows or columns |
| `void insert(int value, int row, int col)` | Set one element |
| `void display() const` | Print the matrix to standard output |
| `Matrix transpose()` | Return the transposed matrix |
| `Matrix operator+(Matrix&)`, `Matrix operator-(Matrix&)` | Element-wise add and subtract. Throws `std::runtime_error` if sizes differ |
| `vector<int> find(int target)` | First `{row, col}` where the value occurs, or an empty vector. Throws `std::logic_error` if the matrix is empty |
| `vector<int> find_sorted(int target)` | Binary search for a matrix sorted in row-major order. Returns `{row, col}` or an empty vector |

### `RGBImage` (`ddimage/ImageProcessor.h`)

```cpp
struct RGBImage {
    Matrix r, g, b;       // one matrix per color channel, values 0-255
    int width, height;
    RGBImage(Matrix red, Matrix green, Matrix blue, int h, int w);   // note: height first
};
```

### `InterpolationMethod`

| Value | Status |
| --- | --- |
| `INTER_NEAREST` | Implemented |
| `INTER_LINEAR` | Implemented (bilinear) |
| `INTER_CUBIC` | Implemented (bicubic) |
| `INTER_AREA` | Implemented (area averaging, best for shrinking) |
| `INTER_MAGIC_21`, `INTER_MAGIC_13` | Reserved. Currently fall back to bilinear |

### `ImageProcessor` (`ddimage/ImageProcessor.h`)

Each function below exists in a `Matrix` (grayscale) and an `RGBImage` (color) version unless noted.

| Function | Description |
| --- | --- |
| `Matrix load(const char* file, int& w, int& h)` | Load any format stb_image can decode (JPEG, PNG, BMP, TGA, and more) as grayscale. On failure it prints a message and returns a 1x1 matrix |
| `RGBImage load_rgb(const char* file, int& w, int& h)` | Same, as color. Color version only |
| `void save(const char* file, Matrix& img, int w, int h)` | Save a grayscale image as PNG |
| `void save(const char* file, RGBImage& img)` | Save a color image as PNG |
| `negative(img)` | Invert every pixel (`255 - value`) |
| `box_blur(img)` | 5x5 uniform averaging blur |
| `Gaussian_blur(img)` | 5x5 Gaussian blur (binomial kernel, divided by 256) |
| `bilateralFilter(img, sigma_space, sigma_colour)` | Edge-preserving smoothing. Neighborhood radius is `int(2 * sigma_space)` |
| `resize(img, new_width, new_height, method)` | Resize with the chosen `InterpolationMethod` |
| `resizeNearest`, `resizeLinear`, `resizeArea`, `resizeCubic` | The individual resize implementations (grayscale versions) |
| `ROI(img, x, y, roi_width, roi_height)` | Crop a rectangle. If it falls outside the image it prints a message and returns a 1x1 matrix |
| `ROI_circular(img, cx, cy, radius)` | Experimental, see [Known limitations](#known-limitations) |

---

## Examples

### Desktop viewer (Qt)

`examples/Image_processing_in_qt_window` is a Qt 6 application called **ImageProcessorApp**. It loads an image (the bundled sample from `examples/data`, or any file you pick) and lists the result of each DDImage operation so you can compare them side by side:

- Resize with nearest, bilinear, and bicubic interpolation, both halving and doubling
- Gaussian blur, box blur, and bilateral filter
- Negative
- Rectangular ROI

It also supports zoom and fit-to-window, saving the current result or all results, and taking a photo with your webcam through Qt Multimedia.

Build and run (requires Qt 6):

```bash
cmake -S . -B build
cmake --build build
```

The executable is created under `build/examples/Image_processing_in_qt_window/`. On macOS the camera-permission `info.plist` is embedded automatically.

### ESP32-CAM live viewer (experimental)

`examples/eps32-CAM_webserver/main.cpp` streams a live **side-by-side view over Wi-Fi**: the original camera frame on the left and the same frame after a DDImage filter on the right. Both halves come from a single capture, so they are always in sync.

How it works:

1. The camera is configured for raw RGB565 output.
2. Each frame is converted into a DDImage `RGBImage`.
3. A filter runs on it (`box_blur` by default, change it in `applyFilter()` at the top of the file).
4. The original and filtered images are placed side by side and JPEG-encoded with `stb_image_write`.
5. The result is served as an MJPEG stream. Open the board's IP address in a browser.

Before you run it:

- Set `WIFI_SSID` and `WIFI_PASSWORD`.
- The pin map is for the **AI-Thinker ESP32-CAM**. Other boards need their own pins.
- Enable **PSRAM** in your board settings.
- Add `include/` to the include path and compile `src/*.cpp` together with the sketch.
- Start with a small frame size (the default is 96x96) and watch the serial monitor, which prints free memory and frames per second.

> **Memory warning:** `Matrix` currently stores one `int` (4 bytes) per pixel per channel, so a color image uses roughly 12 bytes per pixel. That is heavy for a microcontroller and is the main reason frame sizes must stay small for now. Compact pixel storage is the top item on the [Roadmap](#roadmap).

---

## Known limitations

- **Memory use.** `Matrix` is a `vector<vector<int>>` that uses dynamic allocation and 4 bytes per pixel. This is fine on a desktop but limits what fits on a microcontroller.
- **Borders.** The 5x5 blurs leave a 2-pixel border at 0, and `bilateralFilter` leaves a border of `radius` pixels at 0.
- **`ROI_circular` is experimental** and needs fixes. Avoid it for now.
- **Reserved interpolation modes.** `INTER_MAGIC_21` and `INTER_MAGIC_13` are not implemented yet.
- **Tests.** The `tests/` folder is empty for now.
- **Floating point.** Filters use `float` math, which is slow on microcontrollers without an FPU (such as basic Arduino boards).

---

## Roadmap

DDImage is moving toward being an easy-to-use image processing library for embedded projects. Arduino and ESP32 come first, then STM32 and M5Stack.

**Embedded foundations**

- [ ] Compact pixel storage (8-bit instead of `int`) and fixed-size, allocation-free buffers
- [ ] Integer / fixed-point versions of the filters for chips without an FPU
- [ ] Compile-time switches to strip out unused features
- [ ] Platform adapters that turn a camera buffer (for example ESP32 RGB565) into a DDImage image
- [ ] Remove `using namespace std;` from public headers

**Planned functions**

- [ ] Color space conversion: RGB to grayscale, YUV, HSV
- [ ] Thresholding and binarization
- [ ] Sobel edge detection
- [ ] Histogram equalization
- [ ] Morphological operations: erosion and dilation
- [ ] Frame differencing for motion detection
- [ ] Connected components / blob detection
- [ ] Embedded-friendly crop and downsample
- [ ] Generic convolution (3x3 / 5x5 kernel) framework

**Project health**

- [ ] Unit tests
- [ ] Arduino / PlatformIO packaging
- [ ] Continuous integration

---

## Contributing

Contributions are welcome. Fork the repository, create a branch for your change, and open a pull request. If you plan a larger change, such as a new platform adapter, open an issue first so it can be discussed.

---

## License and credits

See [LICENSE](LICENSE).

DDImage bundles [`stb_image.h`](https://github.com/nothings/stb) and `stb_image_write.h` by Sean Barrett and contributors, which are released into the public domain (or under the MIT license, at your option).