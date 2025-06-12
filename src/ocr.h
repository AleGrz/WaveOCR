#ifndef OCR_H
#define OCR_H

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

class Match {
public:
    Match(): character(' '), position(-1, -1), score(-1.0) {}

    Match(const char c, const cv::Point &pos, const double sc)
        : character(c), position(pos), score(sc) {}

    Match(const char c, const cv::Point &pos, const double sc, const cv::Size &sz)
        : character(c), position(pos), score(sc), size(sz) {}

    Match &operator = (const Match &other);

    char character;
    cv::Point position;
    double score;
    cv::Size size;
};


class CharTemplate {
public:
    CharTemplate(const char c, cv::Mat tpl) : character(c), pattern(std::move(tpl)) {}

    char character;
    cv::Mat pattern;
};

void createAlphabet(const std::string &fontPath, const std::string &alphabet);

class OCR {
public:
    OCR() = default;

    void loadFile(const std::string &filePath);

    void findPattern(const cv::Mat &pattern, cv::Mat &result) const;

    double deskew();

    std::string recognize(const std::string &fontName, double threshold = 0.5);

private:
    cv::Mat imageWOB_;
    cv::Mat imageDFT_;
    cv::Mat imageIntegral_;
    cv::Size dftSize_;

    static std::string arrangeOutput(std::vector<Match> &matches, double avgW, double avgH);
    static double getIntersectionOverUnion(const Match &m1, const Match &m2);
};

#endif
