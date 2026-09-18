#if RADIOLIB_EXCLUDE_SX126X != 1
#include "SX126xInterface.h"
#include "configuration.h"
#include "error.h"
#include "mesh/NodeDB.h"
#ifdef ARCH_PORTDUINO
#include "PortduinoGlue.h"
#endif
#if defined(ARCH_ESP32)
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#endif

#include "Throttle.h"

// Particular boards might define a different max power based on what their hardware can do, default to max power output if not
// specified (may be dangerous if using external PA and SX126x power config forgotten)
#if ARCH_PORTDUINO
#define SX126X_MAX_POWER portduino_config.sx126x_max_power
#endif
#ifndef SX126X_MAX_POWER
#define SX126X_MAX_POWER 22
#endif

template <typename T>
SX126xInterface<T>::SX126xInterface(LockingArduinoHal *hal, RADIOLIB_PIN_TYPE cs, RADIOLIB_PIN_TYPE irq, RADIOLIB_PIN_TYPE rst,
                                    RADIOLIB_PIN_TYPE busy)
    : RadioLibInterface(hal, cs, irq, rst, busy, &lora), lora(&module)
{
    LOG_DEBUG("SX126xInterface(cs=%d, irq=%d, rst=%d, busy=%d)", cs, irq, rst, busy);
}

/// Initialise the Driver transport hardware and software.
/// Make sure the Driver is properly configured before calling init().
/// \return true if initialisation succeeded.
template <typename T> bool SX126xInterface<T>::init()
{

// Typically, the RF switch on SX126x boards is controlled by two signals, which are negations of each other (switched RFIO
// paths). The negation is usually performed in hardware, or (suboptimal design) TXEN and RXEN are the two inputs to this style of
// RF switch. On some boards, there is no hardware negation between CTRL and ¬CTRL, but CTRL is internally connected to DIO2, and
// DIO2's switching is done by the SX126X itself, so the MCU can't control ¬CTRL at exactly the same time. One solution would be
// to set ¬CTRL as SX126X_TXEN or SX126X_RXEN, but they may already be used for another purpose, such as controlling another
// PA/LNA. Keeping ¬CTRL high seems to work, as long CTRL=1, ¬CTRL=1 has the opposite and stable RF path effect as CTRL=0 and
// ¬CTRL=1, this depends on the RF switch, but it seems this usually works. Better hardware design, which is done most the time,
// means this workaround is not necessary.
#ifdef SX126X_ANT_SW // Perhaps add RADIOLIB_NC check, and beforehand define as such if it is undefined, but it is not commonly
                     // used and not part of the 'default' set of pin definitions.
    digitalWrite(SX126X_ANT_SW, HIGH);
    pinMode(SX126X_ANT_SW, OUTPUT);
#endif

#ifdef SX126X_POWER_EN
    // Field v2: every MCU boot gives the external SX126x a true cold start.
    pinMode(SX126X_POWER_EN, OUTPUT);
#if defined(FIELD_RECOVERY_V2)
    digitalWrite(SX126X_POWER_EN, LOW);
    delay(100);
#endif
    digitalWrite(SX126X_POWER_EN, HIGH);
#if defined(FIELD_RECOVERY_V2)
    delay(100);
#endif
#endif

#if HAS_LORA_FEM
    loraFEMInterface.init();
    // Apply saved FEM LNA mode from config
    if (loraFEMInterface.isLnaCanControl()) {
        loraFEMInterface.setLNAEnable(config.lora.fem_lna_mode != meshtastic_Config_LoRaConfig_FEM_LNA_Mode_DISABLED);
    }
#endif

#ifdef RF95_FAN_EN
    pinMode(RF95_FAN_EN, OUTPUT);
    digitalWrite(RF95_FAN_EN, HIGH);
#endif

#if ARCH_PORTDUINO
    tcxoVoltage = (float)portduino_config.dio3_tcxo_voltage / 1000;
    if (portduino_config.lora_sx126x_ant_sw_pin.pin != RADIOLIB_NC) {
        digitalWrite(portduino_config.lora_sx126x_ant_sw_pin.pin, HIGH);
        pinMode(portduino_config.lora_sx126x_ant_sw_pin.pin, OUTPUT);
    }
#endif
    if (tcxoVoltage == 0.0)
        LOG_DEBUG("SX126X_DIO3_TCXO_VOLTAGE not defined, not using DIO3 as TCXO reference voltage");
    else
        LOG_DEBUG("SX126X_DIO3_TCXO_VOLTAGE defined, using DIO3 as TCXO reference voltage at %f V", tcxoVoltage);
    setTransmitEnable(false);
    // FIXME: May want to set depending on a definition, currently all SX126x variant files use the DC-DC regulator option
    bool useRegulatorLDO = false; // Seems to depend on the connection to pin 9/DCC_SW - if an inductor DCDC?

    RadioLibInterface::init();

    limitPower(SX126X_MAX_POWER);
    // Make sure we reach the minimum power supported to turn the chip on (-9dBm)
    if (power < -9)
        power = -9;

    int res = lora.begin(getFreq(), bw, sf, cr, syncWord, power, preambleLength, tcxoVoltage, useRegulatorLDO);

#ifdef SX126X_PA_RAMP_US
    // Set custom PA ramp time for boards requiring longer stabilization (e.g., T-Beam 1W needs >800us)
    if (res == RADIOLIB_ERR_NONE) {
        lora.setPaRampTime(SX126X_PA_RAMP_US);
    }
#endif
    // \todo Display actual typename of the adapter, not just `SX126x`
    LOG_INFO("SX126x init result %d", res);
    if (res == RADIOLIB_ERR_CHIP_NOT_FOUND || res == RADIOLIB_ERR_SPI_CMD_FAILED)
        return false;

    LOG_INFO("Frequency set to %f", getFreq());
    LOG_INFO("Bandwidth set to %f", bw);
    LOG_INFO("Power output set to %d", power);

    // Overriding current limit
    // (https://github.com/jgromes/RadioLib/blob/690a050ebb46e6097c5d00c371e961c1caa3b52e/src/modules/SX126x/SX126x.cpp#L85) using
    // value in SX126xInterface.h (currently 140 mA) It may or may not be necessary, depending on how RadioLib functions, from
    // SX1261/2 datasheet: OCP after setting DeviceSel with SetPaConfig(): SX1261 - 60 mA, SX1262 - 140 mA For the SX1268 the IC
    // defaults to 140mA no matter the set power level, but RadioLib set it lower, this would need further checking Default values
    // are: SX1262, SX1268: 0x38 (140 mA), SX1261: 0x18 (60 mA)
    // FIXME: Not ideal to increase SX1261 current limit above 60mA as it can only transmit max 15dBm, should probably only do it
    // if using SX1262 or SX1268
    res = lora.setCurrentLimit(currentLimit);
    LOG_DEBUG("Current limit set to %f", currentLimit);
    LOG_DEBUG("Current limit set result %d", res);

    if (res == RADIOLIB_ERR_NONE) {
#ifdef SX126X_DIO2_AS_RF_SWITCH
        bool dio2AsRfSwitch = true;
#elif defined(ARCH_PORTDUINO)
        bool dio2AsRfSwitch = false;
        if (portduino_config.dio2_as_rf_switch) {
            dio2AsRfSwitch = true;
        }
#else
        bool dio2AsRfSwitch = false;
#endif
        res = lora.setDio2AsRfSwitch(dio2AsRfSwitch);
        LOG_DEBUG("Set DIO2 as %sRF switch, result: %d", dio2AsRfSwitch ? "" : "not ", res);
    }

// If a pin isn't defined, we set it to RADIOLIB_NC, it is safe to always do external RF switching with RADIOLIB_NC as it has
// no effect
#if ARCH_PORTDUINO
    if (res == RADIOLIB_ERR_NONE) {
        LOG_DEBUG("Use MCU pin %i as RXEN and pin %i as TXEN to control RF switching", portduino_config.lora_rxen_pin.pin,
                  portduino_config.lora_txen_pin.pin);
        lora.setRfSwitchPins(portduino_config.lora_rxen_pin.pin, portduino_config.lora_txen_pin.pin);
    }
#else
#ifndef SX126X_RXEN
#define SX126X_RXEN RADIOLIB_NC
    LOG_DEBUG("SX126X_RXEN not defined, defaulting to RADIOLIB_NC");
#endif
#ifndef SX126X_TXEN
#define SX126X_TXEN RADIOLIB_NC
    LOG_DEBUG("SX126X_TXEN not defined, defaulting to RADIOLIB_NC");
#endif
    if (res == RADIOLIB_ERR_NONE) {
        LOG_DEBUG("Use MCU pin %i as RXEN and pin %i as TXEN to control RF switching", SX126X_RXEN, SX126X_TXEN);
        lora.setRfSwitchPins(SX126X_RXEN, SX126X_TXEN);
    }
#endif
    if (config.lora.sx126x_rx_boosted_gain) {
        uint16_t result = lora.setRxBoostedGainMode(true);
        LOG_INFO("Set RX gain to boosted mode; result: %d", result);
    } else {
        uint16_t result = lora.setRxBoostedGainMode(false);
        LOG_INFO("Set RX gain to power saving mode (boosted mode off); result: %d", result);
    }

    // Undocumented SX1262 register patch recommended by Heltec/Semtech for improved RX sensitivity.
    // Sets bit 0 of register 0x8B5.
    if (module.SPIsetRegValue(0x8B5, 0x01, 0, 0) == RADIOLIB_ERR_NONE) {
        LOG_INFO("Applied SX1262 register 0x8B5 patch for RX improvement");
    } else {
        LOG_WARN("Failed to apply SX1262 register 0x8B5 patch for RX improvement");
    }

#if 0
    // Read/write a register we are not using (only used for FSK mode) to test SPI comms
    uint8_t crcLSB = 0;
    int err = lora.readRegister(SX126X_REG_CRC_POLYNOMIAL_LSB, &crcLSB, 1);
    if(err != RADIOLIB_ERR_NONE)
        RECORD_CRITICALERROR(CriticalErrorCode_SX1262Failure);

    //if(crcLSB != 0x0f)
    //    RECORD_CRITICALERROR(CriticalErrorCode_SX1262Failure);

    crcLSB = 0x5a;
    err = lora.writeRegister(SX126X_REG_CRC_POLYNOMIAL_LSB, &crcLSB, 1);
    if(err != RADIOLIB_ERR_NONE)
        RECORD_CRITICALERROR(CriticalErrorCode_SX1262Failure);

    err = lora.readRegister(SX126X_REG_CRC_POLYNOMIAL_LSB, &crcLSB, 1);
    if(err != RADIOLIB_ERR_NONE)
        RECORD_CRITICALERROR(CriticalErrorCode_SX1262Failure);

    if(crcLSB != 0x5a)
        RECORD_CRITICALERROR(CriticalErrorCode_SX1262Failure);
    // If we got this far register accesses (and therefore SPI comms) are good
#endif

    if (res == RADIOLIB_ERR_NONE)
        res = lora.setCRC(RADIOLIB_SX126X_LORA_CRC_ON);

    if (res == RADIOLIB_ERR_NONE)
        startReceive(); // start receiving

    return res == RADIOLIB_ERR_NONE;
}

template <typename T> int16_t SX126xInterface<T>::programModemParams()
{
    int16_t err = lora.setSpreadingFactor(sf);
    if (err != RADIOLIB_ERR_NONE) return err;
    err = lora.setBandwidth(bw);
    if (err != RADIOLIB_ERR_NONE) return err;
    err = lora.setCodingRate(cr);
    if (err != RADIOLIB_ERR_NONE) return err;
    err = lora.setSyncWord(syncWord);
    if (err != RADIOLIB_ERR_NONE) return err;
    err = lora.setCurrentLimit(currentLimit);
    if (err != RADIOLIB_ERR_NONE) return err;
    err = lora.setPreambleLength(preambleLength);
    if (err != RADIOLIB_ERR_NONE) return err;
    err = lora.setFrequency(getFreq());
    if (err != RADIOLIB_ERR_NONE) return err;
    limitPower(SX126X_MAX_POWER);
    if (power < -9) power = -9;
    err = lora.setOutputPower(power);
    if (err != RADIOLIB_ERR_NONE) return err;
    return lora.setRxBoostedGainMode(config.lora.sx126x_rx_boosted_gain);
}

template <typename T> bool SX126xInterface<T>::reconfigure()
{
    RadioLibInterface::reconfigure();
    int16_t err = trySetStandby();
    if (err == RADIOLIB_ERR_NONE)
        err = programModemParams();
    if (err != RADIOLIB_ERR_NONE) {
        LOG_ERROR("SX126x reconfigure failed %s%d; full recovery", radioLibErr, err);
        if (!recoverChipStateLoss())
            return false;
    }
    startReceive();
    return !rxOffline;
}

template <typename T> int16_t SX126xInterface<T>::getCurrentRSSI()
{
    float rssi = lora.getRSSI(false);
    return (int16_t)round(rssi);
}

template <typename T> void SX126xInterface<T>::disableInterrupt()
{
    lora.clearDio1Action();
}

template <typename T> int16_t SX126xInterface<T>::trySetStandby()
{
    checkNotification();
    int16_t err = lora.standby();
    if (err != RADIOLIB_ERR_NONE)
        LOG_WARN("SX126x standby %s%d", radioLibErr, err);
    isReceiving = false;
    activeReceiveStart = 0;
    disableInterrupt();
    completeSending();
    RadioLibInterface::setStandby();
    return err;
}

template <typename T> void SX126xInterface<T>::setStandby()
{
    (void)trySetStandby();
}

/**
 * Add SNR data to received messages
 */
template <typename T> void SX126xInterface<T>::addReceiveMetadata(meshtastic_MeshPacket *mp)
{
    // LOG_DEBUG("PacketStatus %x", lora.getPacketStatus());
    mp->rx_snr = lora.getSNR();
    mp->rx_rssi = lround(lora.getRSSI());
    LOG_DEBUG("Corrected frequency offset: %f", lora.getFrequencyError());
}

/** We override to turn on transmitter power as needed.
 */
template <typename T> void SX126xInterface<T>::configHardwareForSend()
{
    setTransmitEnable(true);
    RadioLibInterface::configHardwareForSend();
}

// For power draw measurements, helpful to force radio to stay sleeping
// #define SLEEP_ONLY

template <typename T> void SX126xInterface<T>::startReceive()
{
#ifdef SLEEP_ONLY
    sleep();
#else
    setTransmitEnable(false);
    auto tryStartRx = [&]() -> int16_t {
        int16_t e = trySetStandby();
        if (e != RADIOLIB_ERR_NONE)
            return e;
        return lora.startReceiveDutyCycleAuto(preambleLength, 8, MESHTASTIC_RADIOLIB_IRQ_RX_FLAGS);
    };

    int16_t err = tryStartRx();
    if (err != RADIOLIB_ERR_NONE) {
        LOG_ERROR("SX126X startReceiveDutyCycleAuto %s%d", radioLibErr, err);
        if (maybeRecoverChipStateLoss())
            err = lora.startReceiveDutyCycleAuto(preambleLength, 8, MESHTASTIC_RADIOLIB_IRQ_RX_FLAGS);
    }
    if (err != RADIOLIB_ERR_NONE) {
        LOG_ERROR("SX126X RX remains offline %s%d", radioLibErr, err);
        rxOffline = true;
        return;
    }

    RadioLibInterface::startReceive();
    enableInterrupt(isrRxLevel0);
    checkRxDoneIrqFlag();
#endif
}

/** Is the channel currently active? */
template <typename T> bool SX126xInterface<T>::isChannelActive()
{
    ChannelScanConfig_t cfg = {.cad = {.symNum = NUM_SYM_CAD,
                                       .detPeak = RADIOLIB_SX126X_CAD_PARAM_DEFAULT,
                                       .detMin = RADIOLIB_SX126X_CAD_PARAM_DEFAULT,
                                       .exitMode = RADIOLIB_SX126X_CAD_PARAM_DEFAULT,
                                       .timeout = 0,
                                       .irqFlags = RADIOLIB_IRQ_CAD_DEFAULT_FLAGS,
                                       .irqMask = RADIOLIB_IRQ_CAD_DEFAULT_MASK}};
    setTransmitEnable(false);
    int16_t result = trySetStandby();
    if (result == RADIOLIB_ERR_NONE) {
        result = lora.scanChannel(cfg);
        if (result == RADIOLIB_LORA_DETECTED) return true;
        if (result == RADIOLIB_CHANNEL_FREE) return false;
        LOG_ERROR("SX126X scanChannel %s%d", radioLibErr, result);
    }
    maybeRecoverChipStateLoss();
    return false;
}

/** Could we send right now (i.e. either not actively receiving or transmitting)? */
template <typename T> bool SX126xInterface<T>::isActivelyReceiving()
{
    // The IRQ status will be cleared when we start our read operation. Check if we've started a header, but haven't yet
    // received and handled the interrupt for reading the packet/handling errors.
    return receiveDetected(lora.getIrqFlags(), RADIOLIB_SX126X_IRQ_HEADER_VALID, RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED);
}

template <typename T> bool SX126xInterface<T>::sleep()
{
    // Not keeping config is busted - next time nrf52 board boots lora sending fails  tcxo related? - see datasheet
    // \todo Display actual typename of the adapter, not just `SX126x`
    LOG_DEBUG("SX126x entering sleep mode"); // (FIXME, don't keep config)
    (void)trySetStandby();                    // Stop pending operations without asserting

    // turn off TCXO if it was powered
    // FIXME - this isn't correct
    // lora.setTCXO(0);

    // put chipset into sleep mode (we've already disabled interrupts by now)
    bool keepConfig = true;
    lora.sleep(keepConfig); // Note: we do not keep the config, full reinit will be needed

#ifdef SX126X_POWER_EN
    digitalWrite(SX126X_POWER_EN, LOW);
#endif

#if HAS_LORA_FEM
    loraFEMInterface.setSleepModeEnable();
#endif

    return true;
}

template <typename T> void SX126xInterface<T>::resetAGC()
{
    if (sendingPacket != NULL || hasQueuedTx() || (isReceiving && isActivelyReceiving()) || isIRQPending())
        return;

    int16_t err = lora.sleep(true);
    if (err == RADIOLIB_ERR_NONE)
        err = lora.standby(RADIOLIB_SX126X_STANDBY_RC, true);
    if (err != RADIOLIB_ERR_NONE) {
        LOG_ERROR("SX126x maintenance sleep/standby failed %s%d", radioLibErr, err);
        rxOffline = true;
        maybeRecoverChipStateLoss();
        return;
    }

    uint8_t calData = RADIOLIB_SX126X_CALIBRATE_ALL;
    err = module.SPIwriteStream(RADIOLIB_SX126X_CMD_CALIBRATE, &calData, 1, true, false);
    if (err != RADIOLIB_ERR_NONE) {
        rxOffline = true;
        maybeRecoverChipStateLoss();
        return;
    }

    module.hal->delay(5);
    uint32_t start = millis();
    while (module.hal->digitalRead(module.getGpio()) && (uint32_t)(millis() - start) <= 50UL)
        module.hal->yield();
    if (module.hal->digitalRead(module.getGpio())) {
        LOG_ERROR("SX126x calibration BUSY timeout");
        rxOffline = true;
        maybeRecoverChipStateLoss();
        return;
    }

    err = lora.calibrateImage(getFreq());
    if (err != RADIOLIB_ERR_NONE) {
        rxOffline = true;
        maybeRecoverChipStateLoss();
        return;
    }

    // Meshtastic #11774: image calibration can continue after the API returns.
    module.hal->delay(50);

#ifdef SX126X_DIO2_AS_RF_SWITCH
    err = lora.setDio2AsRfSwitch(true);
#elif defined(ARCH_PORTDUINO)
    err = portduino_config.dio2_as_rf_switch ? lora.setDio2AsRfSwitch(true) : RADIOLIB_ERR_NONE;
#else
    err = RADIOLIB_ERR_NONE;
#endif
    if (err == RADIOLIB_ERR_NONE)
        err = lora.setRxBoostedGainMode(config.lora.sx126x_rx_boosted_gain);
    if (err == RADIOLIB_ERR_NONE)
        err = module.SPIsetRegValue(0x8B5, 0x01, 0, 0);
    if (err != RADIOLIB_ERR_NONE) {
        LOG_ERROR("SX126x maintenance restore failed %s%d", radioLibErr, err);
        rxOffline = true;
        maybeRecoverChipStateLoss();
        return;
    }
    startReceive();
}

template <typename T> bool SX126xInterface<T>::reinitChip(bool powerCycle)
{
    setTransmitEnable(false);
#ifdef SX126X_POWER_EN
    if (powerCycle) {
        pinMode(SX126X_POWER_EN, OUTPUT);
        digitalWrite(SX126X_POWER_EN, LOW);
        delay(150);
        digitalWrite(SX126X_POWER_EN, HIGH);
        delay(150);
    }
#else
    (void)powerCycle;
#endif

    limitPower(SX126X_MAX_POWER);
    if (power < -9) power = -9;
    const bool useRegulatorLDO = false;
    int16_t res = lora.begin(getFreq(), bw, sf, cr, syncWord, power, preambleLength, tcxoVoltage, useRegulatorLDO);
    if (res != RADIOLIB_ERR_NONE) {
        LOG_ERROR("SX126x recovery begin failed %s%d", radioLibErr, res);
        return false;
    }
#ifdef SX126X_PA_RAMP_US
    lora.setPaRampTime(SX126X_PA_RAMP_US);
#endif
    res = lora.setCurrentLimit(currentLimit);
    if (res != RADIOLIB_ERR_NONE) return false;
#ifdef SX126X_DIO2_AS_RF_SWITCH
    res = lora.setDio2AsRfSwitch(true);
#elif defined(ARCH_PORTDUINO)
    res = lora.setDio2AsRfSwitch(portduino_config.dio2_as_rf_switch);
#else
    res = lora.setDio2AsRfSwitch(false);
#endif
    if (res != RADIOLIB_ERR_NONE) return false;
#if ARCH_PORTDUINO
    lora.setRfSwitchPins(portduino_config.lora_rxen_pin.pin, portduino_config.lora_txen_pin.pin);
#else
    lora.setRfSwitchPins(SX126X_RXEN, SX126X_TXEN);
#endif
    res = programModemParams();
    if (res != RADIOLIB_ERR_NONE) return false;
    res = module.SPIsetRegValue(0x8B5, 0x01, 0, 0);
    if (res != RADIOLIB_ERR_NONE) return false;
    return lora.setCRC(RADIOLIB_SX126X_LORA_CRC_ON) == RADIOLIB_ERR_NONE;
}

template <typename T> bool SX126xInterface<T>::recoverChipStateLoss()
{
    return reinitChip(true);
}

/** Control PA mode for GC1109 FEM - CPS pin selects full PA (txon=true) or bypass mode (txon=false) */
template <typename T> void SX126xInterface<T>::setTransmitEnable(bool txon)
{
#if HAS_LORA_FEM
    if (txon) {
        loraFEMInterface.setTxModeEnable();
    } else {
        loraFEMInterface.setRxModeEnable();
    }
#endif
}

#endif