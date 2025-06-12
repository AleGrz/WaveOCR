#include "ocr.h"

#include <opencv2/opencv.hpp>
#include <opencv2/freetype.hpp>

#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <cmath>

void OCR::loadFile(const std::string& filePath) {
    const cv::Mat loaded = cv::imread(filePath, cv::IMREAD_GRAYSCALE);

    if(loaded.empty()) {
        throw std::runtime_error("Could not load image file: " + filePath);
    }

    cv::Mat denoised;
    cv::fastNlMeansDenoising(loaded, denoised, 20.0, 7, 21);

    cv::Mat image;
    cv::threshold(denoised, image, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    int whites = cv::countNonZero(image);

    if (whites > image.total() / 2) {
        cv::bitwise_not(image, image);
    }
    image.convertTo(imageWOB_, CV_32F, 1.0 / 255.0);
}

void OCR::findPattern(const cv::Mat& pattern, cv::Mat& result) const {

    cv::Mat pattern32f;
    pattern.convertTo(pattern32f, CV_32F);

    int patternW = pattern.cols;
    int patternH = pattern.rows;

    cv::Mat patternPadded(dftSize_, CV_32F, cv::Scalar::all(0));
    pattern32f.copyTo(patternPadded(cv::Rect(0, 0, patternW, patternH)));

    cv::Mat templateDFT;
    cv::dft(patternPadded, templateDFT, cv::DFT_COMPLEX_OUTPUT);

    cv::Mat corrFreq, corr;
    cv::mulSpectrums(imageDFT_, templateDFT, corrFreq, 0, true);
    cv::idft(corrFreq, corr, cv::DFT_REAL_OUTPUT | cv::DFT_SCALE);

    cv::Size resultSize(imageWOB_.cols - patternW + 1, imageWOB_.rows - patternH + 1);

    cv::Mat numerator = corr(cv::Rect(0, 0, resultSize.width, resultSize.height));

    cv::Mat patchNorm(resultSize, CV_64F);
    for (int y = 0; y < patchNorm.rows; ++y) {
        for (int x = 0; x < patchNorm.cols; ++x) {
            double S = imageIntegral_.at<double>(y, x);
            double A = imageIntegral_.at<double>(y, x + patternW);
            double B = imageIntegral_.at<double>(y + patternH, x);
            double C = imageIntegral_.at<double>(y + patternH, x + patternW);
            patchNorm.at<double>(y, x) = C - A - B + S;
        }
    }

    double templNorm = cv::norm(pattern32f, cv::NORM_L2SQR);

    result.create(resultSize, CV_32F);
    for (int y = 0; y < result.rows; ++y) {
        for (int x = 0; x < result.cols; ++x) {
            float numV = numerator.at<float>(y, x);
            double denV = std::sqrt(patchNorm.at<double>(y, x) * templNorm);
            result.at<float>(y, x) = (denV > 1e-5) ? static_cast<float>(numV / denV) : 0.0f;
        }
    }
}

double OCR::deskew() {
    if (imageWOB_.empty()) {
        throw std::runtime_error("Image not loaded.");
    }

    std::vector<cv::Vec4i> lines;
    cv::Mat image8u;
    imageWOB_.convertTo(image8u, CV_8U, 255.0);
    cv::Mat canny;
    cv::Canny(image8u, canny, 50, 150, 3);
    cv::HoughLinesP(canny, lines, 1, CV_PI / 180, 80, 30, 10);
    double angleTotal = 0.0;
    int count = 0;
    for(const auto& line : lines) {
        double angle = std::atan2(static_cast<double>(line[3] - line[1]), static_cast<double>(line[2] - line[0]));
        if (std::abs(angle) < CV_PI / 6) { angleTotal += angle; count++; }
    }
    cv::Mat deskewed8u;
    if (count > 0) {
        const double avgRad = angleTotal / count;
        const double avgDeg = avgRad * 180.0 / CV_PI;
        const cv::Point2f center(static_cast<float>(imageWOB_.cols) / 2.0f, static_cast<float>(imageWOB_.rows) / 2.0f);
        const cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, avgDeg, 1.0);
        cv::warpAffine(imageWOB_, imageWOB_, rotationMatrix,
            imageWOB_.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT,
            cv::Scalar(0));
        cv::threshold(imageWOB_, imageWOB_, 0.5, 1.0, cv::THRESH_BINARY);
        return avgDeg;
    }
    return 0.0;
}

double OCR::getIntersectionOverUnion(const Match& m1, const Match& m2) {
    const cv::Rect r1(m1.position, m1.size);
    const cv::Rect r2(m2.position, m2.size);
    const cv::Rect intersection = r1 & r2;

    if (intersection.area() == 0) return 0.0;

    const double unionArea = r1.area() + r2.area() - intersection.area();
    return static_cast<double>(intersection.area()) / unionArea;
}

std::string OCR::arrangeOutput(std::vector<Match>& matches, const double avgW, const double avgH) {
    if (matches.empty()) {
        return "";
    }

    std::ranges::sort(matches, [](const Match& a, const Match& b) {
        return a.score > b.score;
    });

    std::vector<Match> finalMatches;
    std::vector<bool> isSuppressed(matches.size(), false);
    constexpr double iouThreshold = 0.3;

    for (size_t i = 0; i < matches.size(); ++i) {
        if (isSuppressed[i]) {
            continue;
        }
        finalMatches.push_back(matches[i]);
        for (size_t j = i + 1; j < matches.size(); ++j) {
            if (isSuppressed[j]) {
                continue;
            }
            if (getIntersectionOverUnion(matches[i], matches[j]) > iouThreshold) {
                isSuppressed[j] = true;
            }
        }
    }

    if (finalMatches.empty()) {
        return "";
    }

    std::vector<std::vector<Match>> lines;
    std::vector<bool> assigned(finalMatches.size(), false);

    std::ranges::sort(finalMatches, [](const Match& a, const Match& b){
        return (a.position.y + a.size.height / 2) < (b.position.y + b.size.height / 2);
    });

    for (size_t i = 0; i < finalMatches.size(); ++i) {
        if (assigned[i]) {
            continue;
        }

        std::vector<Match> currentLine;
        currentLine.push_back(finalMatches[i]);
        assigned[i] = true;

        double totalLine = finalMatches[i].position.y + finalMatches[i].size.height / 2.0;

        for (size_t j = i + 1; j < finalMatches.size(); ++j) {
            if (assigned[j]) {
                continue;
            }

            const double avgY = totalLine / static_cast<double>(currentLine.size());
            double y = finalMatches[j].position.y + finalMatches[j].size.height / 2.0;

            if (std::abs(y - avgY) < avgH * 0.35) {
                currentLine.push_back(finalMatches[j]);
                assigned[j] = true;
                totalLine += y;
            }
        }
        lines.push_back(currentLine);
    }

    std::string result;
    const double spaceThreshold = avgW * 0.4;

    for (auto& line : lines) {
        if (line.empty()) continue;

        std::ranges::sort(line, [](const Match& a, const Match& b) {
            return a.position.x < b.position.x;
        });

        if (!result.empty()) {
            result += '\n';
        }

        result += line[0].character;
        for (size_t i = 1; i < line.size(); ++i) {
            const auto& prev = line[i - 1];
            const auto& cur = line[i];

            double gap = cur.position.x - (prev.position.x + prev.size.width);

            if (gap > spaceThreshold) {
                const int spaces = std::max(1, static_cast<int>(std::round(gap / avgW)));
                result += std::string(spaces, ' ');
            }

            result += cur.character;
        }
    }

    return result;
}


std::string OCR::recognize(const std::string& fontName, double threshold) {
    if (imageWOB_.empty()) {
        throw std::runtime_error("Image not loaded.");
    }

    int maxW = 60;
    int maxH = 60;

    dftSize_ = cv::Size(
        cv::getOptimalDFTSize(imageWOB_.cols + maxW - 1),
        cv::getOptimalDFTSize(imageWOB_.rows + maxH - 1)
    );

    cv::Mat image2;
    cv::multiply(imageWOB_, imageWOB_, image2);

    cv::integral(image2, imageIntegral_, CV_64F);

    cv::Mat padded(dftSize_, CV_32F, cv::Scalar::all(0));
    imageWOB_.copyTo(padded(cv::Rect(0, 0, imageWOB_.cols, imageWOB_.rows)));

    cv::dft(padded, imageDFT_, cv::DFT_COMPLEX_OUTPUT);

    std::string fontPath = "./WaveOCR/" + fontName + "/";
    std::vector<CharTemplate> allTemplates;
    double totalW = 0;
    double totalH = 0;

    for (const auto& entry : std::filesystem::directory_iterator(fontPath)) {
        std::string fileName = entry.path().filename().string();
        if (entry.path().extension() == ".png") {
            cv::Mat fontTemplate = cv::imread(entry.path(), cv::IMREAD_GRAYSCALE);
            if (fontTemplate.empty()) {
                std::cerr << "Error loading templates for character: " << fileName << std::endl;
                continue;
            }
            char character = static_cast<char>(std::stoi(fileName.substr(0, fileName.size() - 4)));
            allTemplates.emplace_back(character, fontTemplate);
            totalW += fontTemplate.cols;
            totalH += fontTemplate.rows;
        }
    }
    if (allTemplates.empty()) throw std::runtime_error("No character templates found for font: " + fontName);
    double avgW = totalW / static_cast<double>(allTemplates.size());
    double avgH = totalH / static_cast<double>(allTemplates.size());

    std::map<char, cv::Mat> responsesMap;
    std::map<char, cv::Size> templateSizes;
    for (const auto& tpl : allTemplates) {
        cv::Mat pattern;
        findPattern(tpl.pattern, pattern);
        responsesMap[tpl.character] = pattern;
        templateSizes[tpl.character] = tpl.pattern.size();
    }

    std::vector<Match> finalMatches;
    while (true) {
        Match bestMatch;
        for(auto const& [key, val] : responsesMap) {
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(val, &minVal, &maxVal, &minLoc, &maxLoc);
            if (maxVal > bestMatch.score) {
                bestMatch = {key, maxLoc, maxVal};
            }
        }
        if (bestMatch.score < threshold) {
            break;
        }
        finalMatches.emplace_back(
            bestMatch.character,
            bestMatch.position,
            bestMatch.score,
            templateSizes[bestMatch.character]
        );

        cv::Size size = templateSizes[bestMatch.character];
        cv::Point peak = bestMatch.position;

        cv::Size sizeToSuppress(size.width, static_cast<int>(size.height * 0.7));
        cv::Rect suppressRect(peak, sizeToSuppress);

        for(auto& [key, val] : responsesMap) {
            cv::Rect mapRect(0, 0, val.cols, val.rows);
            cv::Rect valid = suppressRect & mapRect;
            if (valid.area() > 0) {
                cv::rectangle(val, valid, cv::Scalar(0), -1);
            }
        }
    }
    return arrangeOutput(finalMatches, avgW, avgH);
}

Match& Match::operator = (const Match &other) {
    if (this != &other) {
        character = other.character;
        position = other.position;
        score = other.score;
        size = other.size;
    }
    return *this;
}
