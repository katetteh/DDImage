#ifndef ImageProcessor_h
#define ImageProcessor_h
#include <iostream>
#include "matrix.h"

struct RGBImage{
    Matrix r, g, b;
    int width, height;
    RGBImage(Matrix red, Matrix green, Matrix blue, int h, int w)
    : r(red), g(green), b(blue), height(h), width(w) {}
};

enum InterpolationMethod{
    INTER_NEAREST,
    INTER_LINEAR,
    INTER_CUBIC,
    INTER_AREA,
    INTER_MAGIC_21,
    INTER_MAGIC_13,
};

class ImageProcessor{
    public:
        ImageProcessor(){

        }
        Matrix load(const char* filename, int& width, int& height);
        RGBImage load_rgb(const char* filename, int& width, int& height);
        void save(const char* filename, Matrix& img, int width, int height);
        void save(const char* filename, RGBImage& img);
        Matrix negative(Matrix& img);
        RGBImage negative(RGBImage& img);
        Matrix box_blur(Matrix& img);
        RGBImage box_blur(RGBImage& img);
        Matrix Gaussian_blur(Matrix& img);
        RGBImage Gaussian_blur(RGBImage& img);
        Matrix ROI(Matrix& img, int x, int y, int width, int height);
        RGBImage ROI(RGBImage& img, int x, int y, int roi_width,  int roi_height);
        Matrix ROI_circular(Matrix& img, int cx, int cy, int radius);
        RGBImage ROI_circular(RGBImage& img, int cx, int cy, int radius);
        Matrix bilateralFilter(Matrix& img, float sigma_space, float sigma_colour);
        RGBImage bilateralFilter(RGBImage& img, float sigma_space, float sigma_colour);
        // Resize methods for RGB images, used internally by resize()
        Matrix resize(Matrix& img, int new_width, int new_height, InterpolationMethod method);
        RGBImage resize(RGBImage& img, int new_width, int new_height, InterpolationMethod method);
        Matrix resizeNearest(Matrix& img, int new_width, int new_height);
        Matrix resizeLinear(Matrix& img, int new_width, int new_height);
        Matrix resizeArea(Matrix& img, int new_width, int new_height);
        Matrix resizeCubic(Matrix& img, int new_width, int new_height);

};
#endif