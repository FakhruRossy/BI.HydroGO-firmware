#ifndef ANALOG_SENSOR_SERVICE_H
#define ANALOG_SENSOR_SERVICE_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "TaskService.h"

class AnalogSensorService : public TaskService {
public:
    // Konstruktor untuk 2 pin relay
    AnalogSensorService(uint8_t relayPin5V, uint8_t relayPinGND, uint8_t i2cAddress = 0x48);

    void setCompensationTDS(double aVarTDS = 0.0003, double bVarTDS = 0.0636, double cVarTDS = 5.7);
    
    // --- SARAN PERBAIKAN ---
    // Ubah nilai default dengan nilai kalibrasi terakhir agar lebih mendekati akurat
    // jika fungsi setCompensationPH() lupa dipanggil dari main.cpp
    void setCompensationPH(double mVarPH = -8.491, double cVarPH = 35.597);

    double getRawTDSVoltage() const;
    double getRawPHVoltage() const;
    double getFilteredTDSVoltage() const;
    double getFilteredPHVoltage() const;
    double getCalibratedTDSValue(double tempC) const;
    double getCalibratedPHValue() const;
    
    // Fungsi untuk tahu mode apa yang sedang aktif
    bool isPhActiveNow() const;

protected:
    void onStart() override;
    void onUpdate() override;

private:
    // Definisi Channel ADC
    static constexpr uint8_t TDS_ADC_CHANNEL = 0; // TDS di A0
    static constexpr uint8_t PH_ADC_CHANNEL  = 1; // pH di A1
    
    // Konfigurasi waktu
    static constexpr int kAverageTdsWindowSize = 30;
    static constexpr int kAveragePhWindowSize  = 160;           
    static constexpr uint32_t TDS_ACTIVE_DURATION  =  45000;
    static constexpr uint32_t PH_ACTIVE_DURATION   =  90000;
    static constexpr uint32_t TDS_STABLE_READ_START_MS =  5000;
    static constexpr uint32_t TDS_STABLE_READ_END_MS   = 35000;
    static constexpr uint32_t PH_STABLE_READ_START_MS  =  5000;
    static constexpr uint32_t PH_STABLE_READ_END_MS    = 85000;

    // Variabel internal
    uint8_t relayPin5V_;
    uint8_t relayPinGND_;
    uint8_t i2cAddress_;
    bool relayState_ = false; // false = TDS, true = pH
    uint32_t lastSwitchTime_ = 0;

    double computeAverage(const double* buffer, size_t count) const;

    Adafruit_ADS1115 ads_;
    double tdsAvgBuf_[kAverageTdsWindowSize] = {0};
    double phAvgBuf_[kAveragePhWindowSize]  = {0};
    size_t tdsSampleCount_ = 0;
    size_t phSampleCount_  = 0;
    size_t tdsIndex_ = 0;
    size_t phIndex_  = 0;
    double aVarTDS_ = 0.0003;
    double bVarTDS_ = 0.0636;
    double cVarTDS_ = 5.7;
    double mVarPH_ = -8.491; // Diubah sesuai saran
    double cVarPH_ = 35.597; // Diubah sesuai saran
    double rawTDSVoltage_ = 0.0f;
    double rawPHVoltage_ = 0.0f;
    double filteredTDSVoltage_ = 0.0f;
    double filteredPHVoltage_ = 0.0f;
    bool analogInitiated_ = false;
};

#endif
