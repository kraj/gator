/* Copyright (C) 2013-2022 by Arm Limited. All rights reserved. */

#include "HwmonDriver.h"

#include "Logging.h"
#include "lib/String.h"
#include "libsensors/sensors.h"

#include <memory>

// feature->type to input map
static sensors_subfeature_type getInput(const sensors_feature_type type)
{
    switch (type) {
        case SENSORS_FEATURE_IN:
            return SENSORS_SUBFEATURE_IN_INPUT;
        case SENSORS_FEATURE_FAN:
            return SENSORS_SUBFEATURE_FAN_INPUT;
        case SENSORS_FEATURE_TEMP:
            return SENSORS_SUBFEATURE_TEMP_INPUT;
        case SENSORS_FEATURE_POWER:
            return SENSORS_SUBFEATURE_POWER_INPUT;
        case SENSORS_FEATURE_ENERGY:
            return SENSORS_SUBFEATURE_ENERGY_INPUT;
        case SENSORS_FEATURE_CURR:
            return SENSORS_SUBFEATURE_CURR_INPUT;
        case SENSORS_FEATURE_HUMIDITY:
            return SENSORS_SUBFEATURE_HUMIDITY_INPUT;
        default:
            LOG_ERROR("Unsupported hwmon feature %i", type);
            handleException();
    }
}

class HwmonCounter : public DriverCounter {
public:
    HwmonCounter(DriverCounter * next,
                 char const * name,
                 const sensors_chip_name * chip,
                 const sensors_feature * feature);
    ~HwmonCounter() override;

    // Intentionally unimplemented
    HwmonCounter(const HwmonCounter &) = delete;
    HwmonCounter & operator=(const HwmonCounter &) = delete;
    HwmonCounter(HwmonCounter &&) = delete;
    HwmonCounter & operator=(HwmonCounter &&) = delete;

    [[nodiscard]] const char * getLabel() const { return mLabel; }
    [[nodiscard]] const char * getTitle() const { return mTitle; }
    [[nodiscard]] bool isDuplicate() const { return mDuplicate; }
    [[nodiscard]] const char * getDisplay() const { return mDisplay; }
    [[nodiscard]] const char * getCounterClass() const { return mCounterClass; }
    [[nodiscard]] const char * getUnit() const { return mUnit; }
    [[nodiscard]] double getMultiplier() const { return mMultiplier; }

    int64_t read() override;

private:
    void init(const sensors_chip_name * chip, const sensors_feature * feature);

    const sensors_chip_name * mChip;
    const sensors_feature * mFeature;
    char * mLabel;
    const char * mTitle;
    const char * mDisplay;
    const char * mCounterClass;
    const char * mUnit;
    double mPreviousValue;
    double mMultiplier;
    bool mMonotonic, mDuplicate;
};

HwmonCounter::HwmonCounter(DriverCounter * next,
                           char const * const name,
                           const sensors_chip_name * const chip,
                           const sensors_feature * feature)
    : DriverCounter(next, name),
      mChip(chip),
      mFeature(feature),
      mLabel(nullptr),
      mTitle(nullptr),
      mDisplay(nullptr),
      mCounterClass(nullptr),
      mUnit(nullptr),
      mPreviousValue(0.0),
      mMultiplier(1.0),
      mMonotonic(false),
      mDuplicate(false)
{
    mLabel = sensors_get_label(mChip, mFeature);

    switch (mFeature->type) {
        case SENSORS_FEATURE_IN:
            mTitle = "Voltage";
            mDisplay = "maximum";
            mCounterClass = "absolute";
            mUnit = "V";
            mMultiplier = 0.001;
            mMonotonic = false;
            break;
        case SENSORS_FEATURE_FAN:
            mTitle = "Fan";
            mDisplay = "average";
            mCounterClass = "absolute";
            mUnit = "RPM";
            mMultiplier = 1.0;
            mMonotonic = false;
            break;
        case SENSORS_FEATURE_TEMP:
            mTitle = "Temperature";
            mDisplay = "maximum";
            mCounterClass = "absolute";
            mUnit = "°C";
            mMultiplier = 0.001;
            mMonotonic = false;
            break;
        case SENSORS_FEATURE_POWER:
            mTitle = "Power";
            mDisplay = "maximum";
            mCounterClass = "absolute";
            mUnit = "W";
            mMultiplier = 0.000001;
            mMonotonic = false;
            break;
        case SENSORS_FEATURE_ENERGY:
            mTitle = "Energy";
            mDisplay = "accumulate";
            mCounterClass = "delta";
            mUnit = "J";
            mMultiplier = 0.000001;
            mMonotonic = true;
            break;
        case SENSORS_FEATURE_CURR:
            mTitle = "Current";
            mDisplay = "maximum";
            mCounterClass = "absolute";
            mUnit = "A";
            mMultiplier = 0.001;
            mMonotonic = false;
            break;
        case SENSORS_FEATURE_HUMIDITY:
            mTitle = "Humidity";
            mDisplay = "average";
            mCounterClass = "absolute";
            mUnit = "%";
            mMultiplier = 0.001;
            mMonotonic = false;
            break;
        default:
            LOG_ERROR("Unsupported hwmon feature %i", mFeature->type);
            handleException();
    }

    for (auto * counter = static_cast<HwmonCounter *>(next); counter != nullptr;
         counter = static_cast<HwmonCounter *>(counter->getNext())) {
        if (strcmp(mLabel, counter->getLabel()) == 0 && strcmp(mTitle, counter->getTitle()) == 0) {
            mDuplicate = true;
            counter->mDuplicate = true;
            break;
        }
    }
}

HwmonCounter::~HwmonCounter()
{
    free(mLabel);
}

int64_t HwmonCounter::read()
{
    double value;
    double result;
    const sensors_subfeature * subfeature;

    // Keep in sync with the read check in HwmonDriver::readEvents
    subfeature = sensors_get_subfeature(mChip, mFeature, getInput(mFeature->type));
    if (subfeature == nullptr) {
        LOG_ERROR("No input value for hwmon sensor %s", mLabel);
        handleException();
    }

    if (sensors_get_value(mChip, subfeature->number, &value) != 0) {
        LOG_ERROR("Can't get input value for hwmon sensor %s", mLabel);
        handleException();
    }

    result = (mMonotonic ? value - mPreviousValue : value);
    mPreviousValue = value;

    return result;
}

HwmonDriver::HwmonDriver() : PolledDriver("Hwmon")
{
}

HwmonDriver::~HwmonDriver()
{
    sensors_cleanup();
}

void HwmonDriver::readEvents(mxml_node_t * const /*unused*/)
{
    int err = sensors_init(nullptr);
    if (err != 0) {
        LOG_SETUP("Libsensors is disabled\nInitialize failed (%d)", err);
        return;
    }
    sensors_sysfs_no_scaling = 1;

    int chip_nr = 0;
    const sensors_chip_name * chip;
    while ((chip = sensors_get_detected_chips(nullptr, &chip_nr)) != nullptr) {
        int feature_nr = 0;
        const sensors_feature * feature;
        while ((feature = sensors_get_features(chip, &feature_nr)) != nullptr) {
            // Keep in sync with HwmonCounter::read
            // Can this counter be read?
            double value;
            const sensors_subfeature * const subfeature =
                sensors_get_subfeature(chip, feature, getInput(feature->type));
            if ((subfeature == nullptr) || (sensors_get_value(chip, subfeature->number, &value) != 0)) {
                continue;
            }

            // Get the name of the counter
            int len = sensors_snprintf_chip_name(nullptr, 0, chip) + 1;
            const std::unique_ptr<char[]> chip_name {new char[len]};
            sensors_snprintf_chip_name(chip_name.get(), len, chip);

            lib::dyn_printf_str_t name {"hwmon_%s_%d_%d", chip_name.get(), chip_nr, feature->number};
            setCounters(new HwmonCounter(getCounters(), name, chip, feature));
        }
    }
}

void HwmonDriver::writeEvents(mxml_node_t * root) const
{
    root = mxmlNewElement(root, "category");
    mxmlElementSetAttr(root, "name", "Hardware Monitor");

    char buf[1024];
    for (auto * counter = static_cast<HwmonCounter *>(getCounters()); counter != nullptr;
         counter = static_cast<HwmonCounter *>(counter->getNext())) {
        mxml_node_t * node = mxmlNewElement(root, "event");
        mxmlElementSetAttr(node, "counter", counter->getName());
        mxmlElementSetAttr(node, "title", counter->getTitle());
        if (counter->isDuplicate()) {
            mxmlElementSetAttrf(node, "name", "%s (0x%x)", counter->getLabel(), counter->getKey());
        }
        else {
            mxmlElementSetAttr(node, "name", counter->getLabel());
        }
        mxmlElementSetAttr(node, "display", counter->getDisplay());
        mxmlElementSetAttr(node, "class", counter->getCounterClass());
        mxmlElementSetAttr(node, "units", counter->getUnit());
        if (counter->getMultiplier() != 1.0) {
            mxmlElementSetAttrf(node, "multiplier", "%f", counter->getMultiplier());
        }
        if (strcmp(counter->getDisplay(), "average") == 0 || strcmp(counter->getDisplay(), "maximum") == 0) {
            mxmlElementSetAttr(node, "average_selection", "yes");
        }
        mxmlElementSetAttr(node, "series_composition", "overlay");
        mxmlElementSetAttr(node, "rendering_type", "line");
        snprintf(buf,
                 sizeof(buf),
                 "libsensors %s sensor %s (%s)",
                 counter->getTitle(),
                 counter->getLabel(),
                 counter->getName());
        mxmlElementSetAttr(node, "description", buf);
    }
}

void HwmonDriver::start()
{
    for (DriverCounter * counter = getCounters(); counter != nullptr; counter = counter->getNext()) {
        if (!counter->isEnabled()) {
            continue;
        }
        counter->read();
    }
}
