#ifndef CODEC_UTILS_H
#define CODEC_UTILS_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <map>

namespace PiTrac
{
struct CodecParams
{
    std::map<std::string, std::string> params;

    int getInt(const std::string &key, int default_val = 0) const
    {
        auto it = params.find(key);
        if (it != params.end())
        {
            try {
                return std::stoi(it->second);
            } catch (const std::exception &) {
                return default_val;
            }
        }
        return default_val;
    }

    double getDouble(const std::string &key, double default_val = 0.0) const
    {
        auto it = params.find(key);
        if (it != params.end())
        {
            try {
                return std::stod(it->second);
            } catch (const std::exception &) {
                return default_val;
            }
        }
        return default_val;
    }

    std::string getString(const std::string &key, const std::string &default_val = "") const
    {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : default_val;
    }
};

class FrameCodec
{
  public:
    virtual ~FrameCodec() = default;
    virtual std::string getCodecName() const = 0;
    virtual std::vector<uint8_t> encode
    (
        const cv::Mat &frame,
        const CodecParams &params = {}
    ) const = 0;
    virtual cv::Mat decode
    (
        const std::vector<uint8_t> &data
    ) const = 0;
};

// Specific codec implementations
class JpegCodec : public FrameCodec
{
  public:
    std::string getCodecName() const override
    {
        return "jpeg";
    }

    std::vector<uint8_t> encode(const cv::Mat &frame, const CodecParams &params = {}) const override
    {
        std::vector<uint8_t> buffer;
        if (frame.empty())
        {
            return buffer;
        }

        int quality = params.getInt("quality", 95);
        std::vector<int> compression_params = {cv::IMWRITE_JPEG_QUALITY, quality};

        cv::imencode(".jpg", frame, buffer, compression_params);
        return buffer;
    }

    cv::Mat decode(const std::vector<uint8_t> &data) const override
    {
        if (data.empty())
        {
            return cv::Mat();
        }
        return cv::imdecode(data, cv::IMREAD_COLOR);
    }
};

class PngCodec : public FrameCodec
{
  public:
    std::string getCodecName() const override
    {
        return "png";
    }

    std::vector<uint8_t> encode(const cv::Mat &frame, const CodecParams &params = {}) const override
    {
        std::vector<uint8_t> buffer;
        if (frame.empty())
        {
            return buffer;
        }

        int compression = params.getInt("compression", 3);
        std::vector<int> compression_params = {cv::IMWRITE_PNG_COMPRESSION, compression};

        cv::imencode(".png", frame, buffer, compression_params);
        return buffer;
    }

    cv::Mat decode(const std::vector<uint8_t> &data) const override
    {
        if (data.empty())
        {
            return cv::Mat();
        }
        return cv::imdecode(data, cv::IMREAD_COLOR);
    }
};
} // namespace PiTrac

#endif // CODEC_UTILS_H