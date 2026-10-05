#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QScrollArea>
#include <QFileDialog>
#include <QMenuBar>
#include <QPixmap>
#include <QStatusBar>
#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QImage>
#include <QMessageBox>
#include <QShortcut>
#include <QWheelEvent>
#include <QCloseEvent>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QCheckBox>
#include <QDialog>
#include <QStackedWidget>

// ── Camera support (needs Qt6 Multimedia + MultimediaWidgets) ──
#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QImageCapture>
#include <QMediaDevices>
#include <QVideoWidget>

#include <algorithm>
#include <vector>
#include <string>
#include <iostream>
using namespace std;

#include <ddimage/matrix.h>
#include <ddimage/ImageProcessor.h>

// ─── Convert RGBImage → QImage ────────────────────────────────────────────
QImage rgbImageToQImage(RGBImage& img) {
    int h = img.r.numRows();
    int w = img.r.numCols();
    QImage qimg(w, h, QImage::Format_RGB888);
    for (int i = 0; i < h; ++i) {
        uchar* line = qimg.scanLine(i);
        for (int j = 0; j < w; ++j) {
            int idx = j * 3;
            line[idx]     = (uchar)std::max(0, std::min(255, img.r[i][j]));
            line[idx + 1] = (uchar)std::max(0, std::min(255, img.g[i][j]));
            line[idx + 2] = (uchar)std::max(0, std::min(255, img.b[i][j]));
        }
    }
    return qimg;
}

// ─── Convert greyscale Matrix → QImage ────────────────────────────────────
// Qt stores it as-is — every byte you wrote is exactly what appears on screen.
// No interpolation is applied by Qt. This is the honest view of your resize output.
QImage matrixToQImage(Matrix& img) {
    int h = img.numRows();
    int w = img.numCols();
    QImage qimg(w, h, QImage::Format_Grayscale8);
    for (int i = 0; i < h; ++i) {
        uchar* line = qimg.scanLine(i);
        for (int j = 0; j < w; ++j)
            line[j] = (uchar)std::max(0, std::min(255, img[i][j]));
    }
    return qimg;
}

// ─── A unified display entry — RGB or greyscale, already converted ─────────
struct ViewEntry {
    std::string label;
    QImage      image;

    static ViewEntry fromRGB(const std::string& lbl, RGBImage img) {
        return {lbl, rgbImageToQImage(img)};
    }
    static ViewEntry fromMatrix(const std::string& lbl, Matrix img) {
        return {lbl, matrixToQImage(img)};
    }
};

// ─── Run every image processing function on one image file ────────────────
// This is the old body of main(), moved here so it can be called again
// whenever a new image is chosen (the startup file OR a camera photo).
// Returns an empty vector if the file could not be decoded.
std::vector<ViewEntry> buildEntries(const QString& imagePath) {
    std::vector<ViewEntry> entries;

    ImageProcessor processor;
    int width = 0, height = 0;
    const QByteArray imageFilename = imagePath.toLocal8Bit();

    // Load greyscale for resize tests, RGB for filter tests
    Matrix   grey = processor.load(imageFilename.constData(), width, height);
    RGBImage rgb  = processor.load_rgb(imageFilename.constData(), width, height);

    if (width <= 1 || height <= 1) {
        QMessageBox::critical(nullptr, "Unable to open image",
            "The selected file could not be decoded as an image.");
        return entries;
    }

    // Keep expensive comparisons small enough that the UI can start promptly.
    const int maxPreviewDimension = 600;
    const double previewScale = std::min(
        1.0, static_cast<double>(maxPreviewDimension) /
                 std::max(width, height));
    const int previewWidth  = std::max(2, static_cast<int>(width  * previewScale));
    const int previewHeight = std::max(2, static_cast<int>(height * previewScale));
    Matrix previewGrey = processor.resize(
        grey, previewWidth, previewHeight, INTER_AREA);
    RGBImage previewRgb = processor.resize(
        rgb, previewWidth, previewHeight, INTER_AREA);

    int sw = previewWidth / 2,  sh = previewHeight / 2;
    int lw = previewWidth * 2,  lh = previewHeight * 2;

    // ── Resize comparisons — enable Pixel-perfect mode (press P) ──
    entries.push_back(ViewEntry::fromMatrix("Original greyscale",       grey));
    entries.push_back(ViewEntry::fromRGB("Nearest ↓ half",           processor.resize(previewRgb, sw, sh, INTER_NEAREST)));
    entries.push_back(ViewEntry::fromRGB("Bilinear ↓ half",          processor.resize(previewRgb, sw, sh, INTER_LINEAR)));
    entries.push_back(ViewEntry::fromRGB("Bicubic ↓ half",           processor.resize(previewRgb, sw, sh, INTER_CUBIC)));
    entries.push_back(ViewEntry::fromRGB("Nearest ↑ double",         processor.resize(previewRgb, lw, lh, INTER_NEAREST)));
    entries.push_back(ViewEntry::fromRGB("Bilinear ↑ double",        processor.resize(previewRgb, lw, lh, INTER_LINEAR)));
    entries.push_back(ViewEntry::fromRGB("Bicubic ↑ double",         processor.resize(previewRgb, lw, lh, INTER_CUBIC)));

    // ── Filter comparisons (RGB, fit-to-window is fine here) ──
    entries.push_back(ViewEntry::fromRGB("Original RGB",                rgb));
    entries.push_back(ViewEntry::fromRGB("Gaussian blur",               processor.Gaussian_blur(previewRgb)));
    entries.push_back(ViewEntry::fromRGB("Bilateral s=5 c=40",          processor.bilateralFilter(previewRgb, 5.0f, 40.0f)));
    entries.push_back(ViewEntry::fromRGB("Negative",                    processor.negative(previewRgb)));
    entries.push_back(ViewEntry::fromRGB("Box blur",                    processor.box_blur(previewRgb)));
    entries.push_back(ViewEntry::fromRGB("ROI square",                  processor.ROI(previewRgb, 200, 200, 300, 300)));

    return entries;
}

// ─── Camera dialog ─────────────────────────────────────────────────────────
// Flow:  live preview → [Capture] → still shown with the question
//        "Use this photo for the image processing functions?"
//        → [Yes, use this photo]  (dialog accepted, photo() holds the image)
//        → [Retake]               (back to live preview)
//        → [Cancel]               (dialog rejected)
class CameraDialog : public QDialog {
public:
    explicit CameraDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Take a photo");
        resize(780, 660);
        setStyleSheet("background:#1e1e1e; color:#ddd;");

        QVBoxLayout* root = new QVBoxLayout(this);

        // Page 0 = live camera, page 1 = captured still
        stack     = new QStackedWidget;
        videoView = new QVideoWidget;
        stillView = new QLabel;
        stillView->setAlignment(Qt::AlignCenter);
        stillView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        stack->addWidget(videoView);
        stack->addWidget(stillView);
        root->addWidget(stack, 1);

        promptLabel = new QLabel("Use this photo for the image processing functions?");
        promptLabel->setAlignment(Qt::AlignCenter);
        promptLabel->setStyleSheet("font-size:14px; padding:6px;");
        root->addWidget(promptLabel);

        QHBoxLayout* row = new QHBoxLayout;
        captureBtn = makeButton("Capture",             "#2a6496", "#3a7bc8");
        useBtn     = makeButton("Yes, use this photo", "#2a7a46", "#3a9b58");
        retakeBtn  = makeButton("Retake",              "#8a5a1a", "#aa7122");
        cancelBtn  = makeButton("Cancel",              "#555555", "#6a6a6a");
        row->addWidget(captureBtn);
        row->addWidget(useBtn);
        row->addWidget(retakeBtn);
        row->addWidget(cancelBtn);
        root->addLayout(row);

        // ── Camera plumbing ──
        const QCameraDevice device = QMediaDevices::defaultVideoInput();
        camera  = new QCamera(device, this);
        session = new QMediaCaptureSession(this);
        capture = new QImageCapture(this);
        session->setCamera(camera);
        session->setImageCapture(capture);
        session->setVideoOutput(videoView);

        // imageCaptured hands us the frame in memory (no file needed yet)
        connect(capture, &QImageCapture::imageCaptured, this,
                [this](int, const QImage& img) {
                    photoImage = img;
                    showStill();
                });
        connect(capture, &QImageCapture::errorOccurred, this,
                [this](int, QImageCapture::Error, const QString& msg) {
                    QMessageBox::warning(this, "Capture failed", msg);
                });
        connect(camera, &QCamera::errorOccurred, this,
                [this](QCamera::Error, const QString& msg) {
                    QMessageBox::warning(this, "Camera error", msg);
                });

        connect(captureBtn, &QPushButton::clicked, this, [this]{ capture->capture(); });
        connect(retakeBtn,  &QPushButton::clicked, this, [this]{
            photoImage = QImage();
            setLiveMode(true);
        });
        connect(useBtn,     &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);

        setLiveMode(true);
        if (device.isNull())
            captureBtn->setEnabled(false);
        else
            camera->start();
    }

    QImage photo() const { return photoImage; }

protected:
    // Always release the camera when the dialog goes away
    void done(int result) override {
        camera->stop();
        QDialog::done(result);
    }

private:
    QStackedWidget*        stack;
    QVideoWidget*          videoView;
    QLabel*                stillView;
    QLabel*                promptLabel;
    QPushButton *captureBtn, *useBtn, *retakeBtn, *cancelBtn;
    QCamera*               camera;
    QMediaCaptureSession*  session;
    QImageCapture*         capture;
    QImage                 photoImage;

    static QPushButton* makeButton(const QString& text,
                                   const QString& bg, const QString& hover) {
        QPushButton* b = new QPushButton(text);
        b->setStyleSheet(QString(
            "QPushButton { background:%1; color:white; border:none;"
            "              padding:10px; border-radius:6px; font-size:12px; }"
            "QPushButton:hover { background:%2; }").arg(bg, hover));
        return b;
    }

    void setLiveMode(bool live) {
        stack->setCurrentIndex(live ? 0 : 1);
        captureBtn->setVisible(live);
        promptLabel->setVisible(!live);
        useBtn->setVisible(!live);
        retakeBtn->setVisible(!live);
    }

    void showStill() {
        setLiveMode(false);
        stillView->setPixmap(QPixmap::fromImage(photoImage).scaled(
            stillView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
};

// ─── Viewer ────────────────────────────────────────────────────────────────
class ImageViewer : public QMainWindow {
    Q_OBJECT
public:
    ImageViewer(std::vector<ViewEntry> entries, QWidget* parent = nullptr)
        : QMainWindow(parent), entries(std::move(entries)),
          currentIndex(0), scaleFactor(1.0), pixelPerfect(false)
    {
        setWindowTitle("Image Processing Viewer");
        resize(1150, 720);

        QWidget* central = new QWidget;
        QHBoxLayout* layout = new QHBoxLayout(central);
        layout->setContentsMargins(0,0,0,0);
        layout->setSpacing(0);
        setCentralWidget(central);

        // ── Sidebar ──
        QWidget* sidebar = new QWidget;
        sidebar->setFixedWidth(230);
        sidebar->setStyleSheet("background:#1a1a1a;");
        QVBoxLayout* sideLayout = new QVBoxLayout(sidebar);
        sideLayout->setContentsMargins(8,12,8,12);
        sideLayout->setSpacing(6);

        QLabel* sideTitle = new QLabel("Images");
        sideTitle->setStyleSheet("color:#888; font-size:11px; padding:4px 0;");
        sideLayout->addWidget(sideTitle);

        filterList = new QListWidget;
        filterList->setStyleSheet(R"(
            QListWidget { background:#1a1a1a; border:none; color:#ddd; font-size:12px; }
            QListWidget::item { padding:9px 8px; border-radius:6px; }
            QListWidget::item:selected { background:#333; color:#fff; }
            QListWidget::item:hover { background:#2a2a2a; }
        )");
        for (auto& e : this->entries)
            filterList->addItem(QString::fromStdString(e.label));
        filterList->setCurrentRow(0);
        sideLayout->addWidget(filterList);

        // ── Pixel-perfect toggle ──────────────────────────────────────────
        // OFF (default): Qt scales your image to fit the window using its own
        //   interpolation. Good for general viewing but masks resize quality.
        // ON: Qt shows your image at EXACTLY 1 output pixel = 1 screen pixel.
        //   Use this to compare Nearest vs Bilinear vs Bicubic honestly.
        // ─────────────────────────────────────────────────────────────────
        pixelPerfectBox = new QCheckBox("Pixel-perfect 1:1  [P]");
        pixelPerfectBox->setStyleSheet(
            "color:#ccc; font-size:12px; padding:8px 4px;");
        pixelPerfectBox->setToolTip(
            "ON  → your resize output shown exactly, no Qt resampling\n"
            "OFF → image scaled to fit window (Qt's own interpolation)");
        sideLayout->addWidget(pixelPerfectBox);

        sizeLabel = new QLabel("");
        sizeLabel->setStyleSheet("color:#555; font-size:11px; padding:0 4px 8px;");
        sideLayout->addWidget(sizeLabel);

        // ── NEW: camera button ──
        cameraBtn = new QPushButton("Take photo with camera");
        cameraBtn->setStyleSheet(R"(
            QPushButton { background:#7a3a96; color:white; border:none;
                          padding:10px; border-radius:6px; font-size:12px; }
            QPushButton:hover { background:#964ab8; })");
        sideLayout->addWidget(cameraBtn);

        saveBtn = new QPushButton("Save current");
        saveBtn->setStyleSheet(R"(
            QPushButton { background:#2a6496; color:white; border:none;
                          padding:10px; border-radius:6px; font-size:12px; }
            QPushButton:hover { background:#3a7bc8; })");
        sideLayout->addWidget(saveBtn);

        saveAllBtn = new QPushButton("Save all");
        saveAllBtn->setStyleSheet(R"(
            QPushButton { background:#2a7a46; color:white; border:none;
                          padding:10px; border-radius:6px; font-size:12px; }
            QPushButton:hover { background:#3a9b58; })");
        sideLayout->addWidget(saveAllBtn);
        layout->addWidget(sidebar);

        // ── Canvas ──
        imageLabel = new QLabel;
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setStyleSheet("background:#1e1e1e;");
        imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

        scrollArea = new QScrollArea;
        scrollArea->setWidget(imageLabel);
        scrollArea->setWidgetResizable(false);
        scrollArea->setAlignment(Qt::AlignCenter);
        scrollArea->setStyleSheet("background:#1e1e1e; border:none;");
        layout->addWidget(scrollArea, 1);

        // ── Connections ──
        connect(filterList,      &QListWidget::currentRowChanged, this, &ImageViewer::showImage);
        connect(saveBtn,         &QPushButton::clicked,           this, &ImageViewer::saveCurrent);
        connect(saveAllBtn,      &QPushButton::clicked,           this, &ImageViewer::saveAll);
        connect(cameraBtn,       &QPushButton::clicked,           this, &ImageViewer::openCamera);
        connect(pixelPerfectBox, &QCheckBox::toggled,             this, &ImageViewer::onTogglePixelPerfect);

        new QShortcut(QKeySequence(Qt::Key_Up),               this, [this]{ navigate(-1); });
        new QShortcut(QKeySequence(Qt::Key_Down),             this, [this]{ navigate(+1); });
        new QShortcut(QKeySequence(Qt::Key_P),                this, [this]{
            pixelPerfectBox->setChecked(!pixelPerfectBox->isChecked());
        });
        new QShortcut(QKeySequence(Qt::Key_F),                this, this, &ImageViewer::fitToWindow);
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S),     this, this, &ImageViewer::saveCurrent);
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this, this, &ImageViewer::zoomIn);
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this, this, &ImageViewer::zoomOut);
        new QShortcut(QKeySequence(Qt::Key_C),                this, this, &ImageViewer::openCamera);

        setupMenuBar();
        statusBar()->showMessage("P = pixel-perfect  |  F = fit  |  C = camera  |  ↑↓ = switch  |  Ctrl+S = save");
        showImage(0);
    }

protected:
    void closeEvent(QCloseEvent* ev) override {
        auto r = QMessageBox::question(this, "Close?",
            "Unsaved images will be lost. Close anyway?",
            QMessageBox::Yes | QMessageBox::No);
        r == QMessageBox::Yes ? ev->accept() : ev->ignore();
    }
    void wheelEvent(QWheelEvent* ev) override {
        if (ev->modifiers() & Qt::ControlModifier) {
            ev->angleDelta().y() > 0 ? zoomIn() : zoomOut();
            ev->accept();
        } else QMainWindow::wheelEvent(ev);
    }
    void resizeEvent(QResizeEvent* ev) override {
        QMainWindow::resizeEvent(ev);
        if (!currentPixmap.isNull() && !pixelPerfect) fitToWindow();
    }

private slots:
    void showImage(int index) {
        if (index < 0 || index >= (int)entries.size()) return;
        currentIndex = index;
        currentPixmap = QPixmap::fromImage(entries[index].image);
        sizeLabel->setText(QString("%1 × %2 px")
            .arg(entries[index].image.width())
            .arg(entries[index].image.height()));
        pixelPerfect ? showAt1to1() : fitToWindow();
    }

    void onTogglePixelPerfect(bool on) {
        pixelPerfect = on;
        on ? showAt1to1() : fitToWindow();
    }

    // Show at exactly 1:1 — Qt does NO resampling.
    void showAt1to1() {
        if (currentPixmap.isNull()) return;
        imageLabel->setPixmap(currentPixmap);   // no scaling at all
        imageLabel->resize(currentPixmap.size());
        updateStatus("1:1 — no Qt interpolation");
    }

    // ── NEW: open camera, capture, confirm, then rerun every function ──
    void openCamera() {
        if (QMediaDevices::videoInputs().isEmpty()) {
            QMessageBox::warning(this, "No camera",
                "No camera was found on this computer.");
            return;
        }

        CameraDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted)
            return;                       // cancelled, or user did not accept the photo

        QImage photo = dlg.photo();
        if (photo.isNull())
            return;

        // Your ImageProcessor loads from a file path, so write the captured
        // frame to a temp file and feed it through the exact same pipeline
        // that Image1.jpg goes through.
        const QString tmpPath = QDir::temp().filePath("imgproc_camera_capture.jpg");
        if (!photo.convertToFormat(QImage::Format_RGB888).save(tmpPath, "JPG", 95)) {
            QMessageBox::critical(this, "Save failed",
                "Could not write the captured photo to a temporary file.");
            return;
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);
        statusBar()->showMessage("Running image processing functions on camera photo...");
        QApplication::processEvents();
        std::vector<ViewEntry> newEntries = buildEntries(tmpPath);
        QApplication::restoreOverrideCursor();

        if (newEntries.empty()) {
            statusBar()->showMessage("Camera photo could not be processed", 4000);
            return;
        }

        setEntries(std::move(newEntries));
        setWindowTitle("Image Processing Viewer — camera photo");
        statusBar()->showMessage("Processed camera photo", 4000);
    }

    void saveCurrent() {
        QString name = QString::fromStdString(entries[currentIndex].label)
                           .replace(' ','_') + ".png";
        QString path = QFileDialog::getSaveFileName(this,"Save",name,"PNG (*.png);;JPEG (*.jpg)");
        if (path.isEmpty()) return;
        entries[currentIndex].image.save(path);
        statusBar()->showMessage("Saved → " + path, 3000);
    }

    void saveAll() {
        QString dir = QFileDialog::getExistingDirectory(this, "Save all to folder");
        if (dir.isEmpty()) return;
        for (auto& e : entries)
            e.image.save(dir + "/" + QString::fromStdString(e.label).replace(' ','_') + ".png");
        statusBar()->showMessage(QString("Saved %1 images").arg(entries.size()), 4000);
    }

    void zoomIn()  { if (!pixelPerfect) applyZoom(scaleFactor * 1.25); }
    void zoomOut() { if (!pixelPerfect) applyZoom(scaleFactor * 0.80); }
    void fitToWindow() {
        if (currentPixmap.isNull()) return;
        QSize avail = scrollArea->size() - QSize(4,4);
        double sx = (double)avail.width()  / currentPixmap.width();
        double sy = (double)avail.height() / currentPixmap.height();
        applyZoom(std::min(sx, sy));
    }

private:
    std::vector<ViewEntry> entries;
    int     currentIndex;
    double  scaleFactor;
    bool    pixelPerfect;
    QPixmap currentPixmap;

    QLabel*      imageLabel;
    QScrollArea* scrollArea;
    QListWidget* filterList;
    QPushButton* saveBtn;
    QPushButton* saveAllBtn;
    QPushButton* cameraBtn;
    QCheckBox*   pixelPerfectBox;
    QLabel*      sizeLabel;

    // Replace everything shown in the viewer with a new set of results
    void setEntries(std::vector<ViewEntry> newEntries) {
        entries = std::move(newEntries);
        filterList->blockSignals(true);       // avoid showImage() firing mid-rebuild
        filterList->clear();
        for (auto& e : entries)
            filterList->addItem(QString::fromStdString(e.label));
        filterList->setCurrentRow(0);
        filterList->blockSignals(false);
        showImage(0);
    }

    void setupMenuBar() {
        QMenu* v = menuBar()->addMenu("&View");
        v->addAction("Zoom in",       this, &ImageViewer::zoomIn);
        v->addAction("Zoom out",      this, &ImageViewer::zoomOut);
        v->addAction("Fit to window", this, &ImageViewer::fitToWindow);
        v->addAction("Pixel-perfect", [this]{
            pixelPerfectBox->setChecked(!pixelPerfectBox->isChecked());
        });
        QMenu* f = menuBar()->addMenu("&File");
        f->addAction("Take photo with camera", this, &ImageViewer::openCamera);
        f->addAction("Save current",  this, &ImageViewer::saveCurrent);
        f->addAction("Save all",      this, &ImageViewer::saveAll);
    }

    void applyZoom(double factor) {
        scaleFactor = std::max(0.05, std::min(factor, 20.0));
        QSize sz = currentPixmap.size() * scaleFactor;
        // FastTransformation = nearest-neighbour zoom, so zooming in shows
        // hard pixel boundaries instead of Qt's smoothed version.
        imageLabel->setPixmap(currentPixmap.scaled(sz,
            Qt::KeepAspectRatio, Qt::FastTransformation));
        imageLabel->resize(sz);
        updateStatus(QString("Zoom %1%").arg(qRound(scaleFactor * 100)));
    }

    void navigate(int d) {
        int n = currentIndex + d;
        if (n >= 0 && n < (int)entries.size()) filterList->setCurrentRow(n);
    }

    void updateStatus(const QString& extra) {
        auto& e = entries[currentIndex];
        statusBar()->showMessage(QString("%1  |  %2×%3  |  %4  |  %5/%6")
            .arg(QString::fromStdString(e.label))
            .arg(e.image.width()).arg(e.image.height())
            .arg(extra)
            .arg(currentIndex+1).arg(entries.size()));
    }
};

#include "main.moc"

// ─── Entry point ──────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QString imagePath;
    const QStringList candidates = {
        QDir::current().filePath("image1.jpg"),
        QDir(QStringLiteral(DDIMAGE_DATA_DIR)).filePath("image1.jpg"),
        QDir(QCoreApplication::applicationDirPath()).filePath("image1.jpg")
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo(candidate).isFile()) {
            imagePath = QFileInfo(candidate).absoluteFilePath();
            break;
        }
    }

    if (imagePath.isEmpty()) {
        imagePath = QFileDialog::getOpenFileName(
            nullptr, "Open image", QDir::homePath(),
            "Images (*.jpg *.jpeg *.png *.bmp)");
    }

    if (imagePath.isEmpty())
        return 0;

    std::vector<ViewEntry> entries = buildEntries(imagePath);
    if (entries.empty())
        return 1;

    ImageViewer viewer(std::move(entries));
    viewer.show();
    return app.exec();
}