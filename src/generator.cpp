#include "generator.h"

#include <filesystem>

Generator::Generator(const std::string &fontPath) {
        ft2_ = cv::freetype::createFreeType2();
        ft2_->loadFontData(fontPath, 0);
        fontName_ = std::filesystem::path(fontPath).stem().string();
}

void Generator::createTestImage(
    const std::string &text,
    const std::string &outPath,
    double angle,
    int lineSpacing,
    std::tuple<int, int, int> textColor,
    std::tuple<int, int, int> bgColor,
    int noiseAmount
) const {
    int thickness = -1;
    int baseline = 0;

    auto txt = cv::Scalar(std::get<0>(textColor), std::get<1>(textColor), std::get<2>(textColor));
    auto bg = cv::Scalar(std::get<0>(bgColor), std::get<1>(bgColor), std::get<2>(bgColor));

    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    int maxWidth = 0;
    int totalHeight = 0;
    std::vector<cv::Size> lineSizes;

    for (const auto &l: lines) {
        cv::Size sz = ft2_->getTextSize(l, 40, thickness, &baseline);
        maxWidth = std::max(maxWidth, sz.width);
        totalHeight += sz.height + lineSpacing;
        lineSizes.push_back(sz);
    }

    int padding = 10;
    int canvasW = maxWidth + 2 * padding;
    int canvasH = totalHeight + 2 * padding;

    cv::Mat canvas(canvasH, canvasW, CV_8UC3, bg);

    int y = padding;
    for (size_t i = 0; i < lines.size(); ++i) {
        cv::Point textOrg(padding, y + lineSizes[i].height);
        ft2_->putText(canvas, lines[i], textOrg, 40, txt, thickness, cv::LINE_AA, true);
        y += lineSizes[i].height + lineSpacing;
    }

    if (std::abs(angle) > 1e-3) {
        cv::Point2f center(static_cast<float>(canvas.cols) / 2.0f, static_cast<float>(canvas.rows) / 2.0f);
        cv::Mat rotMat = cv::getRotationMatrix2D(center, angle, 1.0);

        cv::Rect bbox = cv::RotatedRect(center, canvas.size(), static_cast<float>(angle)).boundingRect();

        rotMat.at<double>(0, 2) += bbox.width / 2.0 - center.x;
        rotMat.at<double>(1, 2) += bbox.height / 2.0 - center.y;

        cv::warpAffine(canvas, canvas, rotMat, bbox.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, bg);
    }

    cv::Mat noise(canvas.size(), canvas.type());
    if (noiseAmount > 0) {
        cv::randn(noise, cv::Scalar::all(0), cv::Scalar::all(noiseAmount));
    } else {
        noise = cv::Mat::zeros(canvas.size(), canvas.type());
    }

    cv::Mat final;
    cv::addWeighted(canvas, 1.0, noise, 1.0, 0, final);

    cv::imwrite(outPath, final);
}


void Generator::createAlphabet(const std::string &alphabet) const {
    std::filesystem::create_directories("./WaveOCR/" + fontName_ + "/");

    for (const char ch : alphabet) {
        if (ch == ' ') continue;
        std::string chStr(1, ch);
        std::string out = "./WaveOCR/" + fontName_ + '/' + std::to_string(chStr[0]) + ".png";
        const bool isPunct = (ch == '.' || ch == ',' || ch == '?' || ch == '!');
        saveCharacterImage(chStr, out, isPunct);
    }
}

void Generator::saveCharacterImage(const std::string& chStr, const std::string& outPath, const bool isPunctuation) const {
    if (std::filesystem::exists(outPath)) return;

    constexpr int imgHeight = 60;
    constexpr int thickness = -1;
    constexpr int height = 40;

    const cv::Size textSize = ft2_->getTextSize(chStr, height, thickness, nullptr);
    const int baseWidth = std::max(textSize.width, 8);
    const int rightPadding = isPunctuation ? 20 : 8;
    const int totalWidth = baseWidth + rightPadding;

    cv::Mat canvas(imgHeight, totalWidth, CV_8UC1, cv::Scalar(0));
    const cv::Point textPosition(4, height);
    ft2_->putText(canvas, chStr, textPosition, height, cv::Scalar(255), thickness, cv::LINE_AA, true);

    std::vector<cv::Point> nonZero;
    cv::findNonZero(canvas, nonZero);
    const cv::Rect contentBounds = cv::boundingRect(nonZero);
    int finalWidth = std::max(contentBounds.width, isPunctuation ? 16 : 8);
    finalWidth = std::min(finalWidth, canvas.cols - contentBounds.x);

    const cv::Mat finalTemplate = canvas(cv::Rect(contentBounds.x, 0, finalWidth, canvas.rows)).clone();
    cv::imwrite(outPath, finalTemplate);
}