#include "AnalogSensorService.h"
#include <algorithm>

// Konstruktor untuk 2 pin relay
AnalogSensorService::AnalogSensorService(uint8_t relayPin5V, uint8_t relayPinGND, uint8_t i2cAddress)
    : relayPin5V_(relayPin5V), relayPinGND_(relayPinGND), i2cAddress_(i2cAddress) {
}

void AnalogSensorService::setCompensationTDS(double aVarTDS, double bVarTDS, double cVarTDS) {
    aVarTDS_ = aVarTDS;
    bVarTDS_ = bVarTDS;
    cVarTDS_ = cVarTDS;
}

void AnalogSensorService::setCompensationPH(double mVarPH, double cVarPH) {
    mVarPH_ = mVarPH;
    cVarPH_ = cVarPH;
}

bool AnalogSensorService::isPhActiveNow() const {
    return relayState_;
}

void AnalogSensorService::onStart() {
    if (!ads_.begin(i2cAddress_)) {
        Serial.println(F("[ANALOG] Inisialisasi ADS gagal!"));
        analogInitiated_ = false;
        return;
    }
    analogInitiated_ = true;
    ads_.setGain(GAIN_TWOTHIRDS);
    ads_.setDataRate(RATE_ADS1115_8SPS);

    // Setup 2 pin relay
    pinMode(relayPin5V_, OUTPUT);
    pinMode(relayPinGND_, OUTPUT);

    // --- PERBAIKAN: Mulai dengan mode TDS (relay OFF / HIGH) ---
    digitalWrite(relayPin5V_, HIGH);
    digitalWrite(relayPinGND_, HIGH);
    relayState_ = false; // false = TDS
    lastSwitchTime_ = millis();
}

void AnalogSensorService::onUpdate() {
    if (!analogInitiated_) { 
        if(ads_.begin(i2cAddress_)) {
            analogInitiated_ = true;
            ads_.setGain(GAIN_TWOTHIRDS);
            ads_.setDataRate(RATE_ADS1115_8SPS);
        } else {
            return;
        }
    }

    uint32_t now = millis();
    uint32_t elapsed = now - lastSwitchTime_;
    uint32_t relaySwitchInterval = relayState_ ? PH_ACTIVE_DURATION : TDS_ACTIVE_DURATION;

    if (elapsed >= relaySwitchInterval) {
        relayState_ = !relayState_; // Ganti mode

        // --- PERBAIKAN: Logika 2 pin relay dibalik ---
        if (relayState_) { // Ganti ke mode pH (relay ON / LOW)
            digitalWrite(relayPin5V_, LOW);
            digitalWrite(relayPinGND_, LOW);
        } else { // Ganti ke mode TDS (relay OFF / HIGH)
            digitalWrite(relayPin5V_, HIGH);
            digitalWrite(relayPinGND_, HIGH);
        }
        lastSwitchTime_ = now;
        return;
    }
    
    uint32_t STABLE_READ_START_MS = relayState_ ? PH_STABLE_READ_START_MS : TDS_STABLE_READ_START_MS;
    uint32_t STABLE_READ_END_MS   = relayState_ ? PH_STABLE_READ_END_MS   : TDS_STABLE_READ_END_MS;
    if (elapsed < STABLE_READ_START_MS || elapsed > STABLE_READ_END_MS) return;

    int16_t raw = ads_.readADC_SingleEnded(relayState_ ? PH_ADC_CHANNEL : TDS_ADC_CHANNEL);
    double voltage = ads_.computeVolts(raw);

    if (!relayState_) { // TDS active
        rawTDSVoltage_ = voltage;
        tdsAvgBuf_[tdsIndex_] = rawTDSVoltage_;
        tdsIndex_ = (tdsIndex_ + 1) % kAverageTdsWindowSize;
        if (tdsSampleCount_ < kAverageTdsWindowSize) tdsSampleCount_++;
        filteredTDSVoltage_ = computeAverage(tdsAvgBuf_, tdsSampleCount_);
    } else { // pH active
        rawPHVoltage_ = voltage;
        phAvgBuf_[phIndex_] = rawPHVoltage_;
        phIndex_ = (phIndex_ + 1) % kAveragePhWindowSize;
        if (phSampleCount_ < kAveragePhWindowSize) phSampleCount_++;
        filteredPHVoltage_ = computeAverage(phAvgBuf_, phSampleCount_);
    }
}

double AnalogSensorService::computeAverage(const double* buffer, size_t count) const {
    double sum = 0.0f;
    for (size_t i = 0; i < count; ++i) { sum += buffer[i]; }
    return (count > 0) ? (sum / count) : 0.0f;
}

double AnalogSensorService::getRawTDSVoltage() const { if (!analogInitiated_) return -7; return rawTDSVoltage_; }
double AnalogSensorService::getRawPHVoltage() const { if (!analogInitiated_) return -7; return rawPHVoltage_; }
double AnalogSensorService::getFilteredTDSVoltage() const { if (!analogInitiated_) return -7; return filteredTDSVoltage_; }
double AnalogSensorService::getFilteredPHVoltage() const { if (!analogInitiated_) return -7; return filteredPHVoltage_; }

double AnalogSensorService::getCalibratedTDSValue(double tempC) const {
    if (!analogInitiated_) return -7;
    double compensationCoefficient = 1.0 + 0.02 * (tempC - 25.0);
    double compensationVoltage = filteredTDSVoltage_ / compensationCoefficient;
    double calibratedTDSValue = (133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage) * 0.5;
    calibratedTDSValue += (aVarTDS_ * pow(calibratedTDSValue, 2) - bVarTDS_ * calibratedTDSValue - cVarTDS_);

    // --- PERBAIKAN: Batasan 1000 ppm dihapus untuk testing ---
    // if (calibratedTDSValue > 1000) calibratedTDSValue = 1000;
    
    if (calibratedTDSValue < 0) calibratedTDSValue = 0;
    return calibratedTDSValue;
}

double AnalogSensorService::getCalibratedPHValue() const {
    if (!analogInitiated_) return -7;
    double calibratedPHValue = filteredPHVoltage_ * mVarPH_ + cVarPH_;
    if (calibratedPHValue > 20) calibratedPHValue = -1;
    else if (calibratedPHValue > 14) calibratedPHValue = 14;
    else if (calibratedPHValue < 0) calibratedPHValue = 0;
    return calibratedPHValue;
}

