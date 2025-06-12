#ifndef GENERATOR_H
#define GENERATOR_H

#include <opencv2/opencv.hpp>
#include <opencv2/freetype.hpp>
#include <string>

class Generator {
public:
    explicit Generator(const std::string &fontPath);

    void createTestImage(
        const std::string &text,
        const std::string &outPath,
        double angle = 0.0,
        int lineSpacing = 10,
        std::tuple<int, int, int> textColor = std::make_tuple(0, 0, 0),
        std::tuple<int, int, int> bgColor = std::make_tuple(255, 255, 255),
        int noiseAmount = 0
    ) const;

    void createAlphabet(const std::string& alphabet) const;

private:
    cv::Ptr<cv::freetype::FreeType2> ft2_;
    std::string fontName_;
    void saveCharacterImage(const std::string& chStr, const std::string& outPath, bool isPunctuation) const;
};

#endif
