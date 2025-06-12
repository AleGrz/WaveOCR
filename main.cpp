#include "src/ocr.h"
#include "src/generator.h"

#include <pybind11/stl.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(WaveOCR, handle) {
    handle.doc() = "WaveOCR is a proof-of-concept C++ implementation of optical character recognition (OCR) using Fast Fourier Transform";
    py::class_<OCR>(handle, "OCR")
        .def(py::init<>())
        .def("load_file", &OCR::loadFile, "Loads an image from a file path.")
        .def("deskew", &OCR::deskew, "Performs automatic deskewing using Hough Line Transform. It is safe to call when the image is not rotated.")
        .def("recognize", &OCR::recognize, "Performs OCR using a specific font's alphabet.",
             py::arg("font_name"),
             py::arg("threshold") = 0.8);
    py::class_<Generator>(handle, "Generator")
        .def(py::init<const std::string&>())
        .def("create_test_image", &Generator::createTestImage, "Generates a test image and saves it to a file.",
             py::arg("text"),
             py::arg("out_path"),
             py::arg("angle") = 0.0,
             py::arg("line_spacing") = 10,
             py::arg("text_color") = std::make_tuple(0, 0, 0),
             py::arg("bg_color") = std::make_tuple(255, 255, 255),
             py::arg("noise_amount") = 0)
        .def("create_alphabet", &Generator::createAlphabet, "Generates alphabet templates for a specific font.",
             py::arg("alphabet") = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?-");
    }