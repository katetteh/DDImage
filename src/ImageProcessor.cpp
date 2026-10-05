#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic pop
#include <ddimage/ImageProcessor.h>
#include <ddimage/matrix.h>
#include <ddimage/stb_image.h>
#include <ddimage/stb_image_write.h>
#include <iostream>

using namespace std;

Matrix ImageProcessor::load(const char* filename, int& width, int& height){
    int channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 1);
    if (!data){
        cout<< "Failed to load image!";
        return Matrix(1,1);
    }
    Matrix img(height, width);
    for(int i = 0; i < height; ++i){
        for(int j =0; j < width; ++j){
            img[i][j] = data[i*width+j];
        }
    }
    stbi_image_free(data);
    cout<< "Image is loaded!" <<endl<< "Width: "<< width<<endl<< "Height: "<<height;
    return img;

}

RGBImage ImageProcessor::load_rgb(const char* filename, int& width, int& height){
    
    int channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 3);
    if (!data){
        cout<< "Failed to load image!";
        return RGBImage{Matrix(1,1), Matrix(1,1), Matrix(1,1), 1, 1};
    }
    Matrix r(height, width), g(height, width), b(height, width);
    for(int i = 0; i < height; ++i){
        for(int j =0; j < width; ++j){
            int idx = (i * width + j) * 3; 
            r[i][j] = data[idx];
            g[i][j] = data[idx + 1];
            b[i][j] = data[idx + 2];
        }
    }
    stbi_image_free(data);
    cout<< "Image is loaded!" <<endl<< "Width: "<< width<<endl<< "Height: "<<height;
    return {r,g, b, height, width};

}

void ImageProcessor::save(const char* filename, Matrix& img, int width, int height){
    unsigned char* output = new unsigned char[height*width];
    for(int i = 0; i < height; ++i){
        for(int j =0; j < width; ++j){
            output[i*width+j] = img[i][j];
        }
    }
    stbi_write_png(filename, width, height, 1, output, width);
    cout<< "Image saved;";
}
void ImageProcessor::save(const char* filename, RGBImage& img){

    int h = img.r.numRows();
    int w = img.r.numCols();

    unsigned char* output = new unsigned char[h * w * 3];
    for(int i = 0; i < h; ++i){
        for(int j = 0; j < w; ++j){
            int idx = (i * w + j) * 3;
            output[idx] = img.r[i][j];
            output[idx + 1] = img.g[i][j];
            output[idx + 2] = img.b[i][j];
        }
    }
    stbi_write_png(filename, w, h, 3, output, w * 3);
    delete[] output;
}

Matrix ImageProcessor::negative(Matrix& img){
    Matrix neg(img.numRows(), img.numCols());
    for(int i = 0; i < img.numRows(); ++i){
        for(int j =0; j < img.numCols(); ++j){
            neg.insert((255 - img[i][j]),i , j);
        }
    }
    return neg;
}

RGBImage ImageProcessor::negative(RGBImage& img){
    return RGBImage{
        negative(img.r),
        negative(img.g),
        negative(img.b),
        img.height,
        img.width
    };
}

Matrix ImageProcessor::box_blur(Matrix& img){
    float kernel[5][5] = {{1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f}, 
                            {1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f}, 
                            {1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f},
                            {1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f},
                            {1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f, 1.0f/25.0f},  
                        };
    Matrix result(img.numRows(), img.numCols());

    for(int i = 2; i < img.numRows()-2; i++){
        for(int j = 2; j < img.numCols()-2; j++){
            float sum = 0;

            for(int ki = 0; ki < 5; ki++){
                for(int kj = 0; kj < 5; kj++){
                    sum += (float)img[i + ki - 2][j + kj - 2]*kernel[ki][kj];
                }
            }
            result.insert((int)(sum + 0.5f), i, j);
        }
    }
    return result;

}

RGBImage ImageProcessor::box_blur(RGBImage& img){
    return RGBImage{
        box_blur(img.r),
        box_blur(img.g),
        box_blur(img.b),
        img.height,
        img.width
    };
}

//Applying a Gaussian blur with a 5X5 kernel on B/W image
Matrix ImageProcessor::Gaussian_blur(Matrix& img){
    float kernel[5][5] = {
        {1, 4, 6, 4, 1},
        {4, 16, 24, 16, 4},
        {6, 24, 36, 24, 6},
        {4, 16, 24, 16, 4},
        {1, 4, 6, 4, 1}
    };
    for(int i = 0; i < 5; i++){
        for(int j = 0; j < 5; j++){
            kernel[i][j] /= 256.0f;
        }
    }

    Matrix result(img.numRows(), img.numCols());
    for(int i = 2; i < img.numRows() - 2; i++){
        for(int j = 2; j < img.numCols() - 2; j++){
            float sum = 0;
            for(int ki = 0; ki < 5; ki++){
                for(int kj = 0; kj < 5; kj++){
                    sum += (float)img[i + ki -2][j + kj -2]*kernel[ki][kj];
                }
            }
            result.insert((int)(sum + 0.5f), i, j);
        }
    }
    return result;
}
//Applying a Gaussian blur with a 5X5 kernel on coloured image
RGBImage ImageProcessor::Gaussian_blur(RGBImage& img){
    return RGBImage{
        Gaussian_blur(img.r),
        Gaussian_blur(img.g),
        Gaussian_blur(img.b),
        img.height,
        img.width
    };
}
//Creating a Rectangular Region of Interest(ROI) of a B/W image
Matrix ImageProcessor::ROI(Matrix& img, int x, int y, int roi_width, int roi_height){
    if (x + roi_width > img.numCols() || y + roi_height > img.numRows()){
        cout<<"ROI is out of bounds!!";
        return Matrix(1, 1);
    }

    Matrix ROI(roi_height, roi_width);
    for(int i = 0; i < roi_height; i++){
        for (int j = 0; j < roi_width; j++){
            ROI[i][j] = img[y + i][x + j];
        }
    }
    return ROI;
}
// Creating a Rectangular Region of Interest(ROI) of a coloured Image
RGBImage ImageProcessor::ROI(RGBImage& img, int x, int y, int roi_width, int roi_height){
    return RGBImage{
        ROI(img.r, x, y, roi_width, roi_height),
        ROI(img.g, x, y, roi_width, roi_height),
        ROI(img.b, x, y, roi_width, roi_height),
        roi_height,
        roi_width
    };
}

Matrix ImageProcessor::ROI_circular(Matrix& img, int cx, int cy, int radius){
    int x_start = min(0, cx - radius);
    int y_start = min(0, cy - radius);
    int x_end = max(img.numCols(), radius + cx);
    int y_end = max(img.numRows(), radius + cy);

    int roi_width = x_end - x_start;
    int roi_height = y_end - y_start;

    Matrix roi_circular(roi_height, roi_width);
    for(int i = 0; i < roi_height; i++){
        for(int j = 0; j < roi_width; j++){
            int img_x = x_start + j;
            int img_y = y_start + i;

            int dx = img_x - cx;
            int dy = img_y - cy;
            if ((dx*dx + dy*dy) <= radius*radius){
                roi_circular.insert(img[img_y][img_x], i, j);
            }else{
                roi_circular[i][j] = 0;
            }
        }
    }
    return roi_circular;
}

RGBImage ImageProcessor::ROI_circular(RGBImage& img, int cx, int cy, int radius){
    return RGBImage{
        ROI_circular(img.r, cx, cy, radius),
        ROI_circular(img.b, cx, cy, radius),
        ROI_circular(img.b, cx, cy, radius),
        radius*2,
        radius*2
    };
}

Matrix ImageProcessor::bilateralFilter(Matrix& img, float sigma_space, float sigma_colour){

    int radius = (int)(2*sigma_space);
    Matrix result(img.numRows(), img.numCols());
    for(int i = radius; i < img.numRows() - radius; i++){
        for(int j = radius; j < img.numCols() - radius; j++){
            float central_intensity = (float)img[i][j];
            float weighted_sum = 0.0f;
            float weight_total = 0.0f;
            for(int ki = -radius; ki <= radius; ki++){
                for(int kj = -radius; kj <= radius; kj++){
                    float neighbour = img[i + ki][j + kj];
                    //How far is this neighbour from the center pixel?
                    // Uses Euclidean distance squared: ki² + kj², then plugs it into Gaussian: exp(-dist² / 2*sigma_space²)
                    float spatial_dist_sq = ki*ki + kj*kj;
                    float spatial_weight = exp(-spatial_dist_sq/(2.0f*sigma_space*sigma_space));
                    // How different is this neighbour's colour from the center?
                    //Takes the square of the difference in intensity then plugs it into Gaussian: exp(-diff² / 2*sigma_colour²)
                    float colour_diff = neighbour - central_intensity;
                    float range_weight = exp(-colour_diff*colour_diff/(2.0f*sigma_colour*sigma_colour)); 
                    // Bilateral weight = spatial × range
                    float weight = range_weight * spatial_weight;
                    weighted_sum += weight*neighbour;
                    weight_total += weight;
                }
            }
            result.insert((int)((weighted_sum/weight_total) + 0.5f), i, j);
        }
    }
    return result;
}

RGBImage ImageProcessor::bilateralFilter(RGBImage& img, float sigma_space, float sigma_colour){
    return RGBImage{
        bilateralFilter(img.r, sigma_space, sigma_colour),
        bilateralFilter(img.g, sigma_space, sigma_colour),
        bilateralFilter(img.b, sigma_space, sigma_colour),
        img.height,
        img.width
    };
}

//Transformation(RESIZING)

Matrix ImageProcessor::resize(Matrix& img, int new_width, int new_height, InterpolationMethod method){
    switch(method){
        case INTER_NEAREST: return resizeNearest(img, new_width, new_height);
        case INTER_LINEAR: return resizeLinear(img, new_width, new_height);
        case INTER_CUBIC: return resizeCubic(img, new_width, new_height);
        case INTER_AREA: return resizeArea(img, new_width, new_height);
        default: return resizeLinear(img, new_width, new_height);
    }
}

RGBImage ImageProcessor::resize(RGBImage& img, int new_width, int new_height, InterpolationMethod method){
    return RGBImage{
        resize(img.r, new_width, new_height, method),
        resize(img.g, new_width, new_height, method),
        resize(img.b, new_width, new_height, method),
        new_height,
        new_width,
    };
}

Matrix ImageProcessor::resizeLinear(Matrix& img, int new_width, int new_height){
    Matrix result(new_height, new_width);

    float scale_x = (float)(img.numCols() - 1) / (new_width  - 1);
    float scale_y = (float)(img.numRows() - 1) / (new_height - 1);

    for(int i = 0; i < new_height; i++){
        for(int j = 0; j < new_width; j++){
            float src_x = j * scale_x;
            float src_y = i * scale_y;

            int x0 = (int)src_x;
            int y0 = (int)src_y;
            int x1 = std::min(x0 + 1, img.numCols() - 1);
            int y1 = std::min(y0 + 1, img.numRows() - 1);

            float dx = src_x - x0;
            float dy = src_y - y0;

            float top    = img[y0][x0]*(1-dx) + img[y0][x1]*dx;
            float bottom = img[y1][x0]*(1-dx) + img[y1][x1]*dx;

            result[i][j] = (int)(top*(1-dy) + bottom*dy + 0.5f);
        }
    }
    return result;
}

Matrix ImageProcessor::resizeNearest(Matrix& img, int new_width, int new_height){
    Matrix result(new_height, new_width);

    float scale_x = (float)img.numCols() / new_width;
    float scale_y = (float)img.numRows() / new_height;

    for(int i = 0; i < new_height; i++){
        for(int j = 0; j < new_width; j++){
            int src_x = std::min((int)(j * scale_x), img.numCols() - 1);
            int src_y = std::min((int)(i * scale_y), img.numRows() - 1); // FIXED: was scale_x
            result[i][j] = img[src_y][src_x];
        }
    }
    return result;
}

Matrix ImageProcessor::resizeArea(Matrix& img, int new_width, int new_height){
    Matrix result(new_height, new_width);

    float scale_x = (float)img.numCols() / new_width;
    float scale_y = (float)img.numRows() / new_height;

    for(int i = 0; i < new_height; i++){        // FIXED: was for j in outer loop
        for(int j = 0; j < new_width; j++){
            float x_start = j * scale_x;
            float y_start = i * scale_y;
            float x_end   = x_start + scale_x;
            float y_end   = y_start + scale_y;

            float sum = 0.0f, count = 0.0f;
            for(int sy = (int)y_start; sy < (int)y_end && sy < img.numRows(); sy++){
                for(int sx = (int)x_start; sx < (int)x_end && sx < img.numCols(); sx++){
                    sum += img[sy][sx];
                    count++;
                }
            }
            result[i][j] = count > 0 ? (int)(sum/count + 0.5f) : 0;
        }
    }
    return result;
}

static inline double cubic_kernel(double x, double a = -0.5){
    if(x < 0.0) x = -x;

    if(x <= 1.0)
        return (a + 2.0)*x*x*x - (a + 3.0)*x*x + 1.0;
    else if(x < 2.0)
        return a*x*x*x - 5.0*a*x*x + 8.0*a*x - 4.0*a;
    else
        return 0.0;
}

Matrix ImageProcessor::resizeCubic(Matrix& img, int new_width, int new_height){
    int src_w = img.numCols();
    int src_h = img.numRows();

    // FIXED: horizontal pass output must be (src_h rows, new_width cols)
    // because we've resampled horizontally but not vertically yet
    Matrix horizontal(src_h, new_width);

    float scale_x = (float)src_w / new_width;
    for(int i = 0; i < src_h; i++){
        for(int j = 0; j < new_width; j++){
            double src_x = (j + 0.5) * scale_x - 0.5;
            int ix = (int)floor(src_x);
            double frac = src_x - ix;

            double sum = 0.0, wsum = 0.0;
            for(int k = -1; k <= 2; k++){
                int sx = std::max(0, std::min(ix + k, src_w - 1));
                double w = cubic_kernel(frac - k);
                sum  += img[i][sx] * w;   // FIXED: was missing * w
                wsum += w;
            }
            if(wsum != 0.0) sum /= wsum;
            horizontal[i][j] = (int)std::max(0.0, std::min(255.0, sum + 0.5));
        }
    }

    // Vertical pass: (src_h, new_width) → (new_height, new_width)
    Matrix result(new_height, new_width);
    float scale_y = (float)src_h / new_height;

    for(int i = 0; i < new_height; i++){
        for(int j = 0; j < new_width; j++){
            double src_y = (i + 0.5) * scale_y - 0.5;
            int iy = (int)floor(src_y);
            double frac = src_y - iy;

            double sum = 0.0, wsum = 0.0;
            for(int k = -1; k <= 2; k++){
                // FIXED: clamp to src_h-1, not new_height-1
                int sy = std::max(0, std::min(iy + k, src_h - 1));
                double w = cubic_kernel(frac - k);
                sum  += horizontal[sy][j] * w;
                wsum += w;
            }
            if(wsum != 0.0) sum /= wsum;
            result[i][j] = (int)std::max(0.0, std::min(255.0, sum + 0.5));
        }
    }
    return result;
}