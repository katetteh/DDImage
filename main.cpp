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
#include <QString>
#include <QCheckBox>
#include <vector>
#include <string>
#include <iostream>
using namespace std;

#include "matrix.h"
#include "ImageProcessor.h"

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
        // This is THE key control for testing your resize functions.
        //
        // OFF (default): Qt scales your image to fit the window using its own
        //   interpolation. Good for general viewing but masks resize quality.
        //
        // ON: Qt shows your image at EXACTLY 1 output pixel = 1 screen pixel.
        //   Qt does zero resampling. You see ONLY what your resize code produced.
        //   Use this to compare Nearest vs Bilinear vs Bicubic honestly.
        //   Scroll around with the scrollbars or arrow keys.
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

        setupMenuBar();
        statusBar()->showMessage("P = pixel-perfect  |  F = fit  |  ↑↓ = switch  |  Ctrl+S = save");
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
    // FastTransformation at the image's own size = identity operation.
    // Every pixel on screen corresponds to exactly one pixel your code wrote.
    void showAt1to1() {
        if (currentPixmap.isNull()) return;
        imageLabel->setPixmap(currentPixmap);   // no scaling at all
        imageLabel->resize(currentPixmap.size());
        updateStatus("1:1 — no Qt interpolation");
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
    QCheckBox*   pixelPerfectBox;
    QLabel*      sizeLabel;

    void setupMenuBar() {
        QMenu* v = menuBar()->addMenu("&View");
        v->addAction("Zoom in",       this, &ImageViewer::zoomIn);
        v->addAction("Zoom out",      this, &ImageViewer::zoomOut);
        v->addAction("Fit to window", this, &ImageViewer::fitToWindow);
        v->addAction("Pixel-perfect", [this]{
            pixelPerfectBox->setChecked(!pixelPerfectBox->isChecked());
        });
        QMenu* f = menuBar()->addMenu("&File");
        f->addAction("Save current",  this, &ImageViewer::saveCurrent);
        f->addAction("Save all",      this, &ImageViewer::saveAll);
    }

    void applyZoom(double factor) {
        scaleFactor = std::max(0.05, std::min(factor, 20.0));
        QSize sz = currentPixmap.size() * scaleFactor;
        // FastTransformation = nearest-neighbour zoom.
        // This means when you zoom IN to inspect pixels, you see hard pixel
        // boundaries — not Qt's smoothed version. Good for comparing methods.
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

    ImageProcessor processor;
    int width, height;

    // Load greyscale for resize tests, RGB for filter tests
    Matrix   grey = processor.load("Image_1.jpg", width, height);
    RGBImage rgb  = processor.load_rgb("Image_1.jpg", width, height);

    int sw = width  / 2,  sh = height / 2;   // downscale targets
    int lw = width  * 2,  lh = height * 2;   // upscale targets (warning: large)

    std::vector<ViewEntry> entries;

    // ── Resize comparisons — enable Pixel-perfect mode (press P) ──
    // Each result has a different pixel size, confirming your function ran.
    // With pixel-perfect OFF, Qt rescales everything to the same window size
    // and uses its own interpolation — you lose the ability to judge quality.
    // With pixel-perfect ON, you see the raw output of each method directly.
    entries.push_back(ViewEntry::fromMatrix("Original greyscale",       grey));
    entries.push_back(ViewEntry::fromMatrix("Nearest ↓ half",           processor.resize(grey, sw, sh, INTER_NEAREST)));
    entries.push_back(ViewEntry::fromMatrix("Bilinear ↓ half",          processor.resize(grey, sw, sh, INTER_LINEAR)));
    entries.push_back(ViewEntry::fromMatrix("Bicubic ↓ half",           processor.resize(grey, sw, sh, INTER_CUBIC)));
    entries.push_back(ViewEntry::fromMatrix("Nearest ↑ double",         processor.resize(grey, lw, lh, INTER_NEAREST)));
    entries.push_back(ViewEntry::fromMatrix("Bilinear ↑ double",        processor.resize(grey, lw, lh, INTER_LINEAR)));
    entries.push_back(ViewEntry::fromMatrix("Bicubic ↑ double",         processor.resize(grey, lw, lh, INTER_CUBIC)));

    // ── Filter comparisons (RGB, fit-to-window is fine here) ──
    entries.push_back(ViewEntry::fromRGB("Original RGB",                rgb));
    entries.push_back(ViewEntry::fromRGB("Gaussian blur",               processor.Gaussian_blur(rgb)));
    entries.push_back(ViewEntry::fromRGB("Bilateral s=5 c=40",          processor.bilateralFilter(rgb, 5.0f, 40.0f)));
    entries.push_back(ViewEntry::fromRGB("Negative",                    processor.negative(rgb)));
    entries.push_back(ViewEntry::fromRGB("Box blur",                    processor.box_blur(rgb)));
    entries.push_back(ViewEntry::fromRGB("ROI square",                  processor.ROI(rgb, 200, 200, 300, 300)));

    ImageViewer viewer(std::move(entries));
    viewer.show();
    return app.exec();
}