#include "iris/config.hpp"
#include "iris/presentation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iterator>
#include <string>

TEST_CASE("integer scale steps down instead of stretching") {
    const iris::PicturePlacement placement = iris::placePicture(800, 600, 256, 240, iris::ScaleMode::Integer, 4, iris::AspectMode::Pixel);
    REQUIRE(placement.width == 512);
    REQUIRE(placement.height == 480);
    REQUIRE(placement.appliedScale == 2);
    REQUIRE(placement.fellBack);
    REQUIRE_FALSE(placement.fitted);
    REQUIRE(placement.x == 144);
    REQUIRE(placement.y == 60);
    REQUIRE(placement.width <= 800);
    REQUIRE(placement.height <= 600);
    const std::string note = iris::placementNote(iris::ScaleMode::Integer, 4, placement);
    REQUIRE(note.find("4x") != std::string::npos);
    REQUIRE(note.find("2x") != std::string::npos);
}

TEST_CASE("fit chooses the largest integer scale that preserves pixels") {
    const iris::PicturePlacement placement = iris::placePicture(1000, 1000, 256, 240, iris::ScaleMode::Fit, 3, iris::AspectMode::Pixel);
    REQUIRE(placement.appliedScale == 3);
    REQUIRE(placement.width == 768);
    REQUIRE(placement.height == 720);
    REQUIRE_FALSE(placement.fellBack);
    REQUIRE(iris::placementNote(iris::ScaleMode::Fit, 3, placement).empty());
}

TEST_CASE("a picture smaller than one integer scale is fitted without stretching one axis") {
    const iris::PicturePlacement placement = iris::placePicture(200, 100, 256, 240, iris::ScaleMode::Fit, 1, iris::AspectMode::Pixel);
    REQUIRE(placement.fitted);
    REQUIRE(placement.width <= 200);
    REQUIRE(placement.height <= 100);
    REQUIRE(placement.width > 0);
    REQUIRE(placement.height > 0);
    const double sourceAspect = 256.0 / 240.0;
    const double placedAspect = static_cast<double>(placement.width) / static_cast<double>(placement.height);
    REQUIRE(placedAspect > sourceAspect * 0.9);
    REQUIRE(placedAspect < sourceAspect * 1.1);
}

TEST_CASE("4:3 aspect uses a four-by-three box") {
    const iris::PicturePlacement placement =
        iris::placePicture(2000, 2000, 256, 240, iris::ScaleMode::Integer, 2, iris::AspectMode::Television);
    REQUIRE(placement.width == 512);
    REQUIRE(placement.height == 384);
    REQUIRE(placement.width * 3 == placement.height * 4);
    REQUIRE(iris::placementLabel(placement, iris::AspectMode::Television) == "2x 4:3");
}

TEST_CASE("presentation shaders name a stage, a version, and in-range parameters") {
    REQUIRE(std::size(iris::kPresentationShaders) == 5);
    const iris::ShaderDescription& color = iris::kPresentationShaders[4];
    REQUIRE(std::string(color.stage) == "color");
    REQUIRE(color.parameterCount == 4);
    for (const iris::ShaderDescription& shader : iris::kPresentationShaders) {
        REQUIRE(shader.version >= 1);
        REQUIRE(shader.id != nullptr);
        REQUIRE(shader.name != nullptr);
        const std::string stage = shader.stage;
        REQUIRE((stage == "sampling" || stage == "color"));
        for (int index = 0; index < shader.parameterCount; ++index) {
            const iris::ParameterRange& parameter = shader.parameters[index];
            REQUIRE(parameter.defaultValue >= parameter.minimum);
            REQUIRE(parameter.defaultValue <= parameter.maximum);
        }
    }
    REQUIRE(iris::matchesPreset(
        iris::PictureFilter::Nearest, iris::PictureFilter::Nearest, 0.0f, 1.0f, 1.0f, 1.0f, 2.0f, 0.5f));
    REQUIRE_FALSE(iris::matchesPreset(
        iris::PictureFilter::Nearest, iris::PictureFilter::Nearest, 0.2f, 1.0f, 1.0f, 1.0f, 2.0f, 0.5f));
}

TEST_CASE("version 1 configuration gains presentation defaults") {
    const auto parsed = iris::ConfigService::fromJson(
        R"({"version":1,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"}})");
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().version == 7);
    REQUIRE(parsed.value().scaleMode == iris::ScaleMode::Fit);
    REQUIRE(parsed.value().filter == iris::PictureFilter::Nearest);
    REQUIRE(parsed.value().aspect == iris::AspectMode::Pixel);
    REQUIRE(parsed.value().vsync);
    REQUIRE_FALSE(parsed.value().fullscreen);
    const auto saved = iris::ConfigService::fromJson(iris::ConfigService::toJson(parsed.value()));
    REQUIRE(saved.ok());
    REQUIRE(saved.value() == parsed.value());
}

TEST_CASE("version 2 picture settings round-trip and reject values outside their ranges") {
    iris::Config config = iris::ConfigService::defaults();
    config.scaleMode = iris::ScaleMode::Integer;
    config.integerScale = 5;
    config.aspect = iris::AspectMode::Television;
    config.filter = iris::PictureFilter::Crt;
    config.fullscreen = true;
    config.vsync = false;
    config.showPerformance = true;
    config.brightness = -0.25f;
    config.contrast = 1.5f;
    config.saturation = 0.5f;
    config.gamma = 1.25f;
    config.sharpness = 3.0f;
    config.scanline = 0.25f;
    const auto parsed = iris::ConfigService::fromJson(iris::ConfigService::toJson(config));
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value() == config);

    const auto badFilter = iris::ConfigService::fromJson(
        R"({"version":2,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"},"video":{"scaleMode":"fit","integerScale":3,"aspectRatio":"pixel","filter":"lcd","fullscreen":false,"vsync":true,"showPerformance":false,"brightness":0,"contrast":1,"saturation":1,"gamma":1,"sharpness":2,"scanline":0.5}})");
    REQUIRE_FALSE(badFilter.ok());

    const auto badScale = iris::ConfigService::fromJson(
        R"({"version":2,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"},"video":{"scaleMode":"integer","integerScale":7,"aspectRatio":"pixel","filter":"nearest","fullscreen":false,"vsync":true,"showPerformance":false,"brightness":0,"contrast":1,"saturation":1,"gamma":1,"sharpness":2,"scanline":0.5}})");
    REQUIRE_FALSE(badScale.ok());

    const auto badGamma = iris::ConfigService::fromJson(
        R"({"version":2,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"},"video":{"scaleMode":"fit","integerScale":3,"aspectRatio":"4:3","filter":"sharp","fullscreen":false,"vsync":true,"showPerformance":false,"brightness":0,"contrast":1,"saturation":1,"gamma":9,"sharpness":2,"scanline":0.5}})");
    REQUIRE_FALSE(badGamma.ok());

    const auto future = iris::ConfigService::fromJson(
        R"({"version":8,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"}})");
    REQUIRE_FALSE(future.ok());

    iris::Config regionConfig = iris::ConfigService::defaults();
    regionConfig.region = iris::RegionChoice::Dendy;
    const auto regionParsed = iris::ConfigService::fromJson(iris::ConfigService::toJson(regionConfig));
    REQUIRE(regionParsed.ok());
    REQUIRE(regionParsed.value().version == 7);
    REQUIRE(regionParsed.value().region == iris::RegionChoice::Dendy);
    auto regionText = iris::ConfigService::toJson(iris::ConfigService::defaults());
    const auto autoAt = regionText.find("\"auto\"");
    REQUIRE(autoAt != std::string::npos);
    regionText.replace(autoAt, 6, "\"secam\"");
    REQUIRE_FALSE(iris::ConfigService::fromJson(regionText).ok());

    const auto version6 = iris::ConfigService::fromJson(
        R"({"version":6,"ui":{"theme":"blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"},"video":{"scaleMode":"fit","integerScale":3,"aspectRatio":"pixel","filter":"nearest","fullscreen":false,"vsync":true,"showPerformance":false,"brightness":0.0,"contrast":1.0,"saturation":1.0,"gamma":1.0,"sharpness":2.0,"scanline":0.5,"overscan":false},"audio":{"volume":1.0,"muted":false,"device":"","latency":"balanced"},"input":{"a":29,"b":27,"turboA":4,"turboB":22,"select":229,"start":40,"up":82,"down":81,"left":80,"right":79,"openGame":18,"quickSave":62,"quickLoad":64,"pause":41,"fullscreen":40,"reset":21},"library":{"show":true},"controllers":{"port1":"keyboard","port2":"","pad1":{"a":1,"b":0,"turboA":10,"turboB":9,"select":4,"start":6,"up":11,"down":12,"left":13,"right":14},"pad2":{"a":1,"b":0,"turboA":10,"turboB":9,"select":4,"start":6,"up":11,"down":12,"left":13,"right":14}},"emulation":{"region":"auto"}})");
    REQUIRE(version6.ok());
    REQUIRE(version6.value().input.fastForward == 66);
    REQUIRE(version6.value().input.screenshot == 69);
    REQUIRE(version6.value().version == 7);
}
