#include "DilbertViewer.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QPixmap>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

DilbertViewer::DilbertViewer(QWidget* parent)
    : QMainWindow(parent), firstComicDate(1989, 4, 16), lastComicDate(2023, 3, 12) {
    setWindowTitle("Dilbert Viewer");
    resize(800, 600);

    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    titleLabel = new QLabel("No comic loaded", central);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont("Arial", 24, QFont::Bold);
    titleLabel->setFont(titleFont);

    imageLabel = new QLabel(central);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumSize(400, 300);
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageLabel->setScaledContents(false);

    QWidget* buttonWidget = new QWidget(central);
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);

    prevButton = new QPushButton("Previous", buttonWidget);
    randButton = new QPushButton("Random", buttonWidget);
    nextButton = new QPushButton("Next", buttonWidget);

    buttonLayout->addWidget(prevButton);
    buttonLayout->addWidget(randButton);
    buttonLayout->addWidget(nextButton);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(buttonWidget);

    setCentralWidget(central);

    connect(randButton, &QPushButton::clicked, this, &DilbertViewer::showRandom);
    connect(prevButton, &QPushButton::clicked, this, &DilbertViewer::showPrevious);
    connect(nextButton, &QPushButton::clicked, this, &DilbertViewer::showNext);

    QTimer::singleShot(0, this, &DilbertViewer::showRandom);
}

QDate DilbertViewer::generateRandomDate() {
    int daysRange = firstComicDate.daysTo(lastComicDate);
    int randomOffset = QRandomGenerator::global()->bounded(daysRange + 1);
    return firstComicDate.addDays(randomOffset);
}

QString DilbertViewer::generateFilePath(const QDate& date) {
    return QString("./Dilbert/%1/Dilbert_%1-%2-%3.png")
        .arg(date.year(), 4, 10, QChar('0'))
        .arg(date.month(), 2, 10, QChar('0'))
        .arg(date.day(), 2, 10, QChar('0'));
}

void DilbertViewer::loadAndDisplay(const QDate& date) {
    QString path = generateFilePath(date);

    while (!QFile::exists(path)) {
        qDebug() << "Comic does not exist:" << path;
        path = generateFilePath(generateRandomDate());
    }

    currentComic.load(path);
    imageLabel->setPixmap(
        currentComic.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    titleLabel->setText(
        QString("Dilbert: %1-%2-%3").arg(date.year()).arg(date.month()).arg(date.day()));

    currentComicDate = date;
}

void DilbertViewer::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    if (!currentComic.isNull()) {
        imageLabel->setPixmap(
            currentComic.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void DilbertViewer::showRandom() {
    QDate randomDate = generateRandomDate();
    loadAndDisplay(randomDate);
}

void DilbertViewer::showPrevious() {
    if (currentComicDate > firstComicDate) {
        loadAndDisplay(currentComicDate.addDays(-1));
    }
}

void DilbertViewer::showNext() {
    if (currentComicDate < lastComicDate) {
        loadAndDisplay(currentComicDate.addDays(1));
    }
}
