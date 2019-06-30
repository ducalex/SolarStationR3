// This code is modified from Adafruit_ADS1X15

/*=========================================================================
    I2C ADDRESS/BITS
    -----------------------------------------------------------------------*/
    #define ADS1015_ADDRESS                 (0x48)    // 1001 000 (ADDR = GND)
/*=========================================================================*/

/*=========================================================================
    POINTER REGISTER
    -----------------------------------------------------------------------*/
    #define ADS1015_REG_POINTER_MASK        (0x03)
    #define ADS1015_REG_POINTER_CONVERT     (0x00)
    #define ADS1015_REG_POINTER_CONFIG      (0x01)
    #define ADS1015_REG_POINTER_LOWTHRESH   (0x02)
    #define ADS1015_REG_POINTER_HITHRESH    (0x03)
/*=========================================================================*/

/*=========================================================================
    CONFIG REGISTER
    -----------------------------------------------------------------------*/
    #define ADS1015_REG_CONFIG_OS_MASK      (0x8000)
    #define ADS1015_REG_CONFIG_OS_SINGLE    (0x8000)  // Write: Set to start a single-conversion
    #define ADS1015_REG_CONFIG_OS_BUSY      (0x0000)  // Read: Bit = 0 when conversion is in progress
    #define ADS1015_REG_CONFIG_OS_NOTBUSY   (0x8000)  // Read: Bit = 1 when device is not performing a conversion

    #define ADS1015_REG_CONFIG_MODE_MASK    (0x0100)
    #define ADS1015_REG_CONFIG_MODE_CONTIN  (0x0000)  // Continuous conversion mode
    #define ADS1015_REG_CONFIG_MODE_SINGLE  (0x0100)  // Power-down single-shot mode (default)

    #define ADS1015_REG_CONFIG_DR_MASK      (0x00E0)
    #define ADS1015_REG_CONFIG_DR_128SPS    (0x0000)  // 128 samples per second
    #define ADS1015_REG_CONFIG_DR_250SPS    (0x0020)  // 250 samples per second
    #define ADS1015_REG_CONFIG_DR_490SPS    (0x0040)  // 490 samples per second
    #define ADS1015_REG_CONFIG_DR_920SPS    (0x0060)  // 920 samples per second
    #define ADS1015_REG_CONFIG_DR_1600SPS   (0x0080)  // 1600 samples per second (default)
    #define ADS1015_REG_CONFIG_DR_2400SPS   (0x00A0)  // 2400 samples per second
    #define ADS1015_REG_CONFIG_DR_3300SPS   (0x00C0)  // 3300 samples per second

    #define ADS1015_REG_CONFIG_CMODE_MASK   (0x0010)
    #define ADS1015_REG_CONFIG_CMODE_TRAD   (0x0000)  // Traditional comparator with hysteresis (default)
    #define ADS1015_REG_CONFIG_CMODE_WINDOW (0x0010)  // Window comparator

    #define ADS1015_REG_CONFIG_CPOL_MASK    (0x0008)
    #define ADS1015_REG_CONFIG_CPOL_ACTVLOW (0x0000)  // ALERT/RDY pin is low when active (default)
    #define ADS1015_REG_CONFIG_CPOL_ACTVHI  (0x0008)  // ALERT/RDY pin is high when active

    #define ADS1015_REG_CONFIG_CLAT_MASK    (0x0004)  // Determines if ALERT/RDY pin latches once asserted
    #define ADS1015_REG_CONFIG_CLAT_NONLAT  (0x0000)  // Non-latching comparator (default)
    #define ADS1015_REG_CONFIG_CLAT_LATCH   (0x0004)  // Latching comparator

    #define ADS1015_REG_CONFIG_CQUE_MASK    (0x0003)
    #define ADS1015_REG_CONFIG_CQUE_1CONV   (0x0000)  // Assert ALERT/RDY after one conversions
    #define ADS1015_REG_CONFIG_CQUE_2CONV   (0x0001)  // Assert ALERT/RDY after two conversions
    #define ADS1015_REG_CONFIG_CQUE_4CONV   (0x0002)  // Assert ALERT/RDY after four conversions
    #define ADS1015_REG_CONFIG_CQUE_NONE    (0x0003)  // Disable the comparator and put ALERT/RDY in high state (default)

    typedef enum
    {
        ADS_GAIN_TWOTHIRDS    = 0x0000,  // +/-6.144V range = Gain 2/3
        ADS_GAIN_ONE          = 0x0200,  // +/-4.096V range = Gain 1
        ADS_GAIN_TWO          = 0x0400,  // +/-2.048V range = Gain 2 (default)
        ADS_GAIN_FOUR         = 0x0600,  // +/-1.024V range = Gain 4
        ADS_GAIN_EIGHT        = 0x0800,  // +/-0.512V range = Gain 8
        ADS_GAIN_SIXTEEN      = 0x0A00   // +/-0.256V range = Gain 16
    } ads1x15_gain_t;


    typedef enum
    {
        ADS_SINGLE_0 = 0x4000,  // Single-ended AIN0
        ADS_SINGLE_1 = 0x5000,  // Single-ended AIN1
        ADS_SINGLE_2 = 0x6000,  // Single-ended AIN2
        ADS_SINGLE_3 = 0x7000,  // Single-ended AIN3
        ADS_DIFF_0_1 = 0x0000,  // Differential P = AIN0, N = AIN1 (default)
        ADS_DIFF_0_3 = 0x1000,  // Differential P = AIN0, N = AIN3
        ADS_DIFF_1_3 = 0x2000,  // Differential P = AIN1, N = AIN3
        ADS_DIFF_2_3 = 0x3000,  // Differential P = AIN2, N = AIN3
    } ads1x15_channel_t;
/*=========================================================================*/


typedef enum
{
    ADS1015 = 0,
    ADS1115 = 1
} ads1x15_type_t;


typedef struct {
   uint8_t   i2c_num;
   uint8_t   i2c_address;
   uint8_t   conversion_delay;
   uint8_t   bit_shift;
   ads1x15_gain_t gain;
} ads1x15_dev_t;
