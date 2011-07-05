/**
 * @file main.c
 *
 * @brief  MAC Example - Star Network
 *
 * This is the source code of a simple MAC example. It implements the
 * firmware for all nodes of a network with star topology.
 *
 * $Id: main.c 24937 2011-01-18 04:02:51Z yogesh.bellan $
 *
 * @author    Atmel Corporation: http://www.atmel.com
 * @author    Support email: avr@atmel.com
 */
/*
 * Copyright (c) 2009, Atmel Corporation All rights reserved.
 *
 * Licensed under Atmel's Limited License Agreement --> EULA.txt
 */

/* === INCLUDES ============================================================ */

#include <AVR2025.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pal.h>
#include <tal.h>
#include <mac_api.h>
#include <app_config.h>
#include <ieee_const.h>

/* === TYPES =============================================================== */

typedef struct associated_device_tag
{
    uint16_t short_addr;
    uint64_t ieee_addr;
}
/** This type definition of a structure can store the short address and the
 *  extended address of a device.
 */
associated_device_t;

/* === MACROS ============================================================== */
#ifdef CHANNEL
#define DEFAULT_CHANNEL                 (CHANNEL)
#define DEFAULT_CHANNEL_PAGE            (0)
#else
/** Defines the default channel. */
#if (TAL_TYPE == AT86RF212)
    #ifdef CHINESE_BAND
        #define DEFAULT_CHANNEL                 (0)
        #define DEFAULT_CHANNEL_PAGE            (5)
    #else
        #define DEFAULT_CHANNEL                 (1)
        #define DEFAULT_CHANNEL_PAGE            (0)
    #endif  /* #ifdef CHINESE_BAND */
#else
#define DEFAULT_CHANNEL                 (20)
#define DEFAULT_CHANNEL_PAGE            (0)
#endif  /* #if (TAL_TYPE == AT86RF212) */
#endif  /* #ifdef CHANNEL */
/** Defines the PAN ID of the network. */
#ifdef PAN_ID
#define DEFAULT_PAN_ID                  (PAN_ID)
#else
#define DEFAULT_PAN_ID                  CCPU_ENDIAN_TO_LE16(0xBABE)
#endif
/** Defines the short address of the coordinator. */
#define COORD_SHORT_ADDR                (0x0000)
/** Defines the maximum number of devices the coordinator will handle. */
#define MAX_NUMBER_OF_DEVICES           (2)
/** This is the time period in micro seconds for data transmissions. */
#define DATA_TX_PERIOD                  (2000000)
/** Defines the bit mask of channels that should be scanned. */
#if (TAL_TYPE == AT86RF212)
    #ifdef CHINESE_BAND
        #define SCAN_ALL_CHANNELS           (0x0000000F)
    #else
        #if (DEFAULT_CHANNEL == 0)
            #define SCAN_ALL_CHANNELS       (0x00000001)
        #else
            #define SCAN_ALL_CHANNELS       (0x000007FE)
        #endif
    #endif  /* #ifdef CHINESE_BAND */
#else
#define SCAN_ALL_CHANNELS               (0x07FFF800)
#endif

/** Defines the scan duration time. */
#define SCAN_DURATION                   (4)
/** Defines the maximum number of scans before starting own network. */
#define MAX_NUMBER_OF_SCANS             (3)

#if (NO_OF_LEDS >= 3)
#define LED_START                       (LED_0)
#define LED_NWK_SETUP                   (LED_1)
#define LED_DATA                        (LED_2)
#elif (NO_OF_LEDS == 2)
#define LED_START                       (LED_0)
#define LED_NWK_SETUP                   (LED_0)
#define LED_DATA                        (LED_1)
#else
#define LED_START                       (LED_0)
#define LED_NWK_SETUP                   (LED_0)
#define LED_DATA                        (LED_0)
#endif

/* === GLOBALS ============================================================= */

/** This structure stores the short and extended address of the coordinator. */
static associated_device_t coord_addr;
/** Number of done network scans */
static uint8_t number_of_scans;
/** This array stores all device related information. */
static associated_device_t device_list[MAX_NUMBER_OF_DEVICES];

/* === PROTOTYPES ========================================================== */

static void app_timer_cb(void *parameter);
static void network_scan_indication_cb(void *parameter);
static void data_exchange_led_off_cb(void *parameter);
static bool assign_new_short_addr(uint64_t addr64, uint16_t *addr16);

/* === IMPLEMENTATION ====================================================== */

/**
 * @brief Main function of the device application
 */
int example_main(void)
{
    printf("Init MAC...\n\r");
    /* Initialize the MAC layer and its underlying layers, like PAL, TAL, BMM. */
    if (wpan_init() != MAC_SUCCESS)
    {
        /*
         * Stay here; we need a valid IEEE address.
         * Check kit documentation how to create an IEEE address
         * and to store it into the EEPROM.
         */
        pal_alert();
    }

    /* Initialize LEDs. */
    printf("Init LEDs...\n\r");
    pal_led_init();
    pal_led(LED_START, LED_ON);         // indicating application is started
    pal_led(LED_NWK_SETUP, LED_OFF);    // indicating node is associated
    pal_led(LED_DATA, LED_OFF);         // indicating successfull data transmission

    /*
     * The stack is initialized above, hence the global interrupts are enabled
     * here.
     */
    printf("Init IRQs...\n\r");
    pal_global_irq_enable();

    /*
     * Reset the MAC layer to the default values
     * This request will cause a mlme reset confirm message ->
     * usr_mlme_reset_conf
     */
    printf("Reset MAC...\n\r");
    wpan_mlme_reset_req(true);

    printf("Ready...\n\r");
    /* Main loop */
    while (1)
    {
        wpan_task();
    }
    return 0;
}

/**
 * @brief Callback function usr_mlme_reset_conf
 *
 * @param status Result of the reset procedure
 */
void usr_mlme_reset_conf(uint8_t status)
{
    if (status == MAC_SUCCESS)
    {
	printf("Reset MAC successful, scanning...\n\r");
        /*
         * Initiate an active scan over all channels to determine
         * which channel is used by the coordinator.
         * Use: bool wpan_mlme_scan_req(uint8_t ScanType,
         *                              uint32_t ScanChannels,
         *                              uint8_t ScanDuration,
         *                              uint8_t ChannelPage);
         *
         * This request leads to a scan confirm message -> usr_mlme_scan_conf
         * Scan for about 50 ms on each channel -> ScanDuration = 1
         * Scan for about 1/2 second on each channel -> ScanDuration = 5
         * Scan for about 1 second on each channel -> ScanDuration = 6
         */
        wpan_mlme_scan_req(MLME_SCAN_TYPE_ACTIVE,
                           SCAN_ALL_CHANNELS,
                           SCAN_DURATION,
                           DEFAULT_CHANNEL_PAGE);

        // Indicate network scanning by a LED flashing
        pal_timer_start(TIMER_LED_OFF,
                        500000,
                        TIMEOUT_RELATIVE,
                        (FUNC_PTR)network_scan_indication_cb,
                        NULL);
    }
    else
    {
	printf("Reset MAC failed, restarting...\n\r");
        // something went wrong; restart
        wpan_mlme_reset_req(true);
    }
}


/**
 * @brief Callback function usr_mlme_scan_conf
 *
 * @param status            Result of requested scan operation
 * @param ScanType          Type of scan performed
 * @param ChannelPage       Channel page on which the scan was performed
 * @param UnscannedChannels Bitmap of unscanned channels
 * @param ResultListSize    Number of elements in ResultList
 * @param ResultList        Pointer to array of scan results
 */
void usr_mlme_scan_conf(uint8_t status,
                        uint8_t ScanType,
                        uint8_t ChannelPage,
                        uint32_t UnscannedChannels,
                        uint8_t ResultListSize,
                        void *ResultList)
{
    number_of_scans++;

    if (status == MAC_SUCCESS)
    {
        wpan_pandescriptor_t *coordinator;
        uint8_t i;
	
	printf("Scan succeeded...\n\r");

        /*
         * Analyze the ResultList.
         * Assume that the first entry of the result list is our coodinator.
         */
        coordinator = (wpan_pandescriptor_t *)ResultList;
        for (i = 0; i < ResultListSize; i++)
        {
            /*
             * Check if the PAN descriptor belongs to our coordinator.
             * Check if coordinator allows association.
             */
            if ((coordinator->LogicalChannel == DEFAULT_CHANNEL) &&
                (coordinator->ChannelPage == DEFAULT_CHANNEL_PAGE) &&
                (coordinator->CoordAddrSpec.PANId == DEFAULT_PAN_ID) &&
                ((coordinator->SuperframeSpec & ((uint16_t)1 << ASSOC_PERMIT_BIT_POS)) == ((uint16_t)1 << ASSOC_PERMIT_BIT_POS))
               )
            {
                // Store the coordinator's address
                if (coordinator->CoordAddrSpec.AddrMode == WPAN_ADDRMODE_SHORT)
                {
                    ADDR_COPY_DST_SRC_16(coord_addr.short_addr, coordinator->CoordAddrSpec.Addr.short_address);
                }
                else if (coordinator->CoordAddrSpec.AddrMode == WPAN_ADDRMODE_LONG)
                {
                    ADDR_COPY_DST_SRC_64(coord_addr.ieee_addr, coordinator->CoordAddrSpec.Addr.long_address);
                }
                else
                {
                    // Something went wrong; restart
		    printf("Error, unknown address mode, restarting...\n\r");
                    wpan_mlme_reset_req(true);
                    return;
                }

                /*
                 * Associate to our coordinator
                 * Use: bool wpan_mlme_associate_req(uint8_t LogicalChannel,
                 *                                   uint8_t ChannelPage,
                 *                                   wpan_addr_spec_t *CoordAddrSpec,
                 *                                   uint8_t CapabilityInformation);
                 * This request will cause a mlme associate confirm message ->
                 * usr_mlme_associate_conf
                 */
                wpan_mlme_associate_req(coordinator->LogicalChannel,
                                        coordinator->ChannelPage,
                                        &(coordinator->CoordAddrSpec),
                                        WPAN_CAP_ALLOCADDRESS);
		printf("Found coordinator, associating...\n\r");
                return;
            }

            // Get the next PAN descriptor
            coordinator++;
        }

        /*
         * If here, the result list does not contain our expected coordinator.
         * Let's scan again.
         */
        if (number_of_scans < MAX_NUMBER_OF_SCANS)
        {
            wpan_mlme_scan_req(MLME_SCAN_TYPE_ACTIVE,
                               SCAN_ALL_CHANNELS,
                               SCAN_DURATION,
                               DEFAULT_CHANNEL_PAGE);
	    printf("Scanning again (unexpected coordinator)...\n\r");
        }
        else
        {
            // No network could be found; therefore start new network.
            /*
             * Set the short address of this node.
             * Use: bool wpan_mlme_set_req(uint8_t PIBAttribute,
             *                             void *PIBAttributeValue);
             *
             * This request leads to a set confirm message -> usr_mlme_set_conf
             */
            uint8_t short_addr[2];

            short_addr[0] = (uint8_t)COORD_SHORT_ADDR;          // low byte
            short_addr[1] = (uint8_t)(COORD_SHORT_ADDR >> 8);   // high byte
            wpan_mlme_set_req(macShortAddress, short_addr);
	    printf("No network found, starting new one (unexpected coordinator)...\n\r");
        }
    }
    else if (status == MAC_NO_BEACON)
    {
        /*
         * No beacon is received; no coordiantor is located.
         * Scan again, but used longer scan duration.
         */
        if (number_of_scans < MAX_NUMBER_OF_SCANS)
        {
            wpan_mlme_scan_req(MLME_SCAN_TYPE_ACTIVE,
                               SCAN_ALL_CHANNELS,
                               SCAN_DURATION,
                               DEFAULT_CHANNEL_PAGE);
	    printf("No network found, scanning again...\n\r");
        }
        else
        {
            // No network could be found; therefore start new network.
            /*
             * Set the short address of this node.
             * Use: bool wpan_mlme_set_req(uint8_t PIBAttribute,
             *                             void *PIBAttributeValue);
             *
             * This request leads to a set confirm message -> usr_mlme_set_conf
             */
            uint8_t short_addr[2];

            short_addr[0] = (uint8_t)COORD_SHORT_ADDR;          // low byte
            short_addr[1] = (uint8_t)(COORD_SHORT_ADDR >> 8);   // high byte
            wpan_mlme_set_req(macShortAddress, short_addr);
	    
	    printf("No network found, starting new one (no beacon)...\n\r");
        }
    }
    else
    {
	printf("Scan failed, restarting...\n\r");
        // Something went wrong; restart
        wpan_mlme_reset_req(true);
    }

    /* Keep compiler happy. */
    ScanType = ScanType;
    UnscannedChannels = UnscannedChannels;
    ChannelPage = ChannelPage;
}


/**
 * @brief Callback function indicating network search
 */
static void network_scan_indication_cb(void *parameter)
{
    printf("Still scanning...\n\r");

    pal_led(LED_NWK_SETUP, LED_TOGGLE);

    // Re-start led timer again
    pal_timer_start(TIMER_LED_OFF,
                    500000,
                    TIMEOUT_RELATIVE,
                    (FUNC_PTR)network_scan_indication_cb,
                    NULL);

    parameter = parameter; /* Keep compiler happy. */
}


/* === Node starts new network === */


/**
 * @brief Callback function usr_mlme_set_conf
 *
 * @param status        Result of requested PIB attribute set operation
 * @param PIBAttribute  Updated PIB attribute
 */
void usr_mlme_set_conf(uint8_t status, uint8_t PIBAttribute)
{
    if ((status == MAC_SUCCESS) && (PIBAttribute == macShortAddress))
    {
        /*
         * Allow other devices to associate to this coordinator.
         * Use: bool wpan_mlme_set_req(uint8_t PIBAttribute,
         *                             void *PIBAttributeValue);
         *
         * This request leads to a set confirm message -> usr_mlme_set_conf
         */
         uint8_t association_permit = true;

         wpan_mlme_set_req(macAssociationPermit, &association_permit);
    }
    else if ((status == MAC_SUCCESS) && (PIBAttribute == macAssociationPermit))
    {
        /*
         * Set RX on when idle to enable the receiver as default.
         * Use: bool wpan_mlme_set_req(uint8_t PIBAttribute,
         *                             void *PIBAttributeValue);
         *
         * This request leads to a set confirm message -> usr_mlme_set_conf
         */
         bool rx_on_when_idle = true;

         wpan_mlme_set_req(macRxOnWhenIdle, &rx_on_when_idle);
    }
    else if ((status == MAC_SUCCESS) && (PIBAttribute == macRxOnWhenIdle))
    {
        /*
         * Start a nonbeacon-enabled network
         * Use: bool wpan_mlme_start_req(uint16_t PANId,
         *                               uint8_t LogicalChannel,
         *                               uint8_t ChannelPage,
         *                               uint8_t BeaconOrder,
         *                               uint8_t SuperframeOrder,
         *                               bool PANCoordinator,
         *                               bool BatteryLifeExtension,
         *                               bool CoordRealignment)
         *
         * This request leads to a start confirm message -> usr_mlme_start_conf
         */
         wpan_mlme_start_req(DEFAULT_PAN_ID,
                             DEFAULT_CHANNEL,
                             DEFAULT_CHANNEL_PAGE,
                             15, 15,
                             true, false, false);
    }
    else
    {
        // something went wrong; restart
        wpan_mlme_reset_req(true);
    }
}


/**
 * @brief Callback function usr_mlme_start_conf
 *
 * @param status        Result of requested start operation
 */
void usr_mlme_start_conf(uint8_t status)
{
    if (status == MAC_SUCCESS)
    {
	printf("Network established.\n\r+OK\n\r");
        /*
         * Network is established.
         * Waiting for association indication from a device.
         * -> usr_mlme_associate_ind
         */
         // Stop timer used for search indication
         pal_timer_stop(TIMER_LED_OFF);
         pal_led(LED_NWK_SETUP, LED_ON);
    }
    else
    {
	printf("Failed to establish network, restarting...\n\r");
        // something went wrong; restart
        wpan_mlme_reset_req(true);
    }
}


/**
 * @brief Callback function usr_mlme_associate_ind
 *
 * @param DeviceAddress         Extended address of device requesting association
 * @param CapabilityInformation Capabilities of device requesting association
 */
void usr_mlme_associate_ind(uint64_t DeviceAddress,
                            uint8_t CapabilityInformation)
{
    /*
     * Any device is allowed to join the network
     * Use: bool wpan_mlme_associate_resp(uint64_t DeviceAddress,
     *                                    uint16_t AssocShortAddress,
     *                                    uint8_t status);
     *
     * This response leads to comm status indication -> usr_mlme_comm_status_ind
     * Get the next available short address for this device
     */
    uint16_t associate_short_addr = macShortAddress_def;

    if (assign_new_short_addr(DeviceAddress, &associate_short_addr) == true)
    {
        wpan_mlme_associate_resp(DeviceAddress,
                                 associate_short_addr,
                                 ASSOCIATION_SUCCESSFUL);
    }
    else
    {
        wpan_mlme_associate_resp(DeviceAddress,
                                 associate_short_addr,
                                 PAN_AT_CAPACITY);
    }

    /* Keep compiler happy. */
    CapabilityInformation = CapabilityInformation;
}


/**
 * @brief Callback function usr_mlme_comm_status_ind
 *
 * @param SrcAddrSpec      Pointer to source address specification
 * @param DstAddrSpec      Pointer to destination address specification
 * @param status           Result for related response operation
 */
void usr_mlme_comm_status_ind(wpan_addr_spec_t *SrcAddrSpec,
                              wpan_addr_spec_t *DstAddrSpec,
                              uint8_t status)
{
    if (status == MAC_SUCCESS)
    {
        /*
         * Now the association of the device has been successful and its
         * information, like address, could  be stored.
         * But for the sake of simple handling it has been done
         * during assignment of the short address within the function
         * assign_new_short_addr()
         */
	printf("Device associated.\n\r+OK\n\r");
    }

    /* Keep compiler happy. */
    SrcAddrSpec = SrcAddrSpec;
    DstAddrSpec = DstAddrSpec;
}


/**
 * @brief Callback function usr_mcps_data_ind
 *
 * @param SrcAddrSpec      Pointer to source address specification
 * @param DstAddrSpec      Pointer to destination address specification
 * @param msduLength       Number of octets contained in MSDU
 * @param msdu             Pointer to MSDU
 * @param mpduLinkQuality  LQI measured during reception of the MPDU
 * @param DSN              DSN of the received data frame.
 * @param Timestamp        The time, in symbols, at which the data were received
 *                         (only if timestamping is enabled).
 */
void usr_mcps_data_ind(wpan_addr_spec_t *SrcAddrSpec,
                       wpan_addr_spec_t *DstAddrSpec,
                       uint8_t msduLength,
                       uint8_t *msdu,
                       uint8_t mpduLinkQuality,
#ifdef ENABLE_TSTAMP
                       uint8_t DSN,
                       uint32_t Timestamp)
#else
                       uint8_t DSN)
#endif  /* ENABLE_TSTAMP */
{
    printf("Received data\n\r");
   /*
    * Dummy data has been received successfully.
    * Application code could be added here ...
    */
    pal_led(LED_DATA, LED_ON);
    // Start a timer switching off the LED
    pal_timer_start(TIMER_LED_OFF,
                    500000,
                    TIMEOUT_RELATIVE,
                    (FUNC_PTR)data_exchange_led_off_cb,
                    NULL);

    /* Keep compiler happy. */
    SrcAddrSpec = SrcAddrSpec;
    DstAddrSpec = DstAddrSpec;
    msduLength = msduLength;
    msdu = msdu;
    mpduLinkQuality = mpduLinkQuality;
    DSN = DSN;
#ifdef ENABLE_TSTAMP
    Timestamp = Timestamp;
#endif  /* ENABLE_TSTAMP */
}


/**
 * @brief Application specific function to assign a short address
 *
 */
static bool assign_new_short_addr(uint64_t addr64, uint16_t *addr16)
{
    uint8_t i;

    // Check if device has been associated before
    for (i = 0; i < MAX_NUMBER_OF_DEVICES; i++)
    {
        if (device_list[i].short_addr == 0x0000)
        {
            // If the short address is 0x0000, it has not been used before
            continue;
        }
        if (device_list[i].ieee_addr == addr64)
        {
            // Assign the previously assigned short address again
            *addr16 = device_list[i].short_addr;
            return true;
        }
    }

    for (i = 0; i < MAX_NUMBER_OF_DEVICES; i++)
    {
        if (device_list[i].short_addr == 0x0000)
        {
            *addr16 = CPU_ENDIAN_TO_LE16(i + 0x0001);
            device_list[i].short_addr = CPU_ENDIAN_TO_LE16(i + 0x0001); // get next short address
            device_list[i].ieee_addr = addr64;      // store extended address
            return true;
        }
    }

    // If we are here, no short address could be assigned.
    return false;
}


/* ===  Node joined existing network === */


/**
 * @brief Callback function usr_mlme_associate_conf
 *
 * @param AssocShortAddress    Short address allocated by the coordinator
 * @param status               Result of requested association operation
 */
void usr_mlme_associate_conf(uint16_t AssocShortAddress, uint8_t status)
{
    if (status == MAC_SUCCESS)
    {
	printf("Joined network.\n\r");
        // Stop timer used for search indication (same as used for data transmission)
        pal_timer_stop(TIMER_LED_OFF);
        pal_led(LED_NWK_SETUP, LED_ON);

        // Start a timer that sends some data to the coordinator every 2 seconds.
        pal_timer_start(TIMER_TX_DATA,
                        DATA_TX_PERIOD,
                        TIMEOUT_RELATIVE,
                        (FUNC_PTR)app_timer_cb,
                        NULL);
    }
    else
    {
        // Something went wrong; restart
        wpan_mlme_reset_req(true);
    }

    /* Keep compiler happy. */
    AssocShortAddress = AssocShortAddress;
}


/**
 * @brief Callback function for the application timer
 *
 * @param AssocShortAddress    Short address allocated by the coordinator
 * @param status               Result of requested association operation
 */
static void app_timer_cb(void *parameter)
{
    /*
     * Send some data and restart timer.
     * Use: bool wpan_mcps_data_req(uint8_t SrcAddrMode,
     *                              wpan_addr_spec_t *DstAddrSpec,
     *                              uint8_t msduLength,
     *                              uint8_t *msdu,
     *                              uint8_t msduHandle,
     *                              uint8_t TxOptions);
     *
     * This request will cause a mcps data confirm message ->
     * usr_mcps_data_conf
     */

    uint8_t src_addr_mode;
    wpan_addr_spec_t dst_addr;
    uint8_t payload;
    static uint8_t msduHandle = 0;

    src_addr_mode = WPAN_ADDRMODE_SHORT;

    dst_addr.AddrMode = WPAN_ADDRMODE_SHORT;
    dst_addr.PANId = DEFAULT_PAN_ID;
    ADDR_COPY_DST_SRC_16(dst_addr.Addr.short_address, coord_addr.short_addr);

    payload = (uint8_t)rand();  // any dummy data
    msduHandle++;               // increment handle
    wpan_mcps_data_req(src_addr_mode,
                       &dst_addr,
                       1,
                       &payload,
                       msduHandle,
                       WPAN_TXOPT_ACK);

    pal_timer_start(TIMER_TX_DATA,
                    DATA_TX_PERIOD,
                    TIMEOUT_RELATIVE,
                    (FUNC_PTR)app_timer_cb,
                    NULL);
    
    printf("Sending data...\n\r");

    parameter = parameter;  /* Keep compiler happy. */
}


/**
 * Callback function usr_mcps_data_conf
 *
 * @param msduHandle  Handle of MSDU handed over to MAC earlier
 * @param status      Result for requested data transmission request
 * @param Timestamp   The time, in symbols, at which the data were transmitted
 *                    (only if timestamping is enabled).
 *
 */
#ifdef ENABLE_TSTAMP
void usr_mcps_data_conf(uint8_t msduHandle, uint8_t status, uint32_t Timestamp)
#else
void usr_mcps_data_conf(uint8_t msduHandle, uint8_t status)
#endif  /* ENABLE_TSTAMP */
{
    if (status == MAC_SUCCESS)
    {
	printf("Data sent.\n\r");
        /*
         * Dummy data has been transmitted successfully.
         * Application code could be added here ...
         */
        pal_led(LED_DATA, LED_ON);
        // Start a timer switching off the LED
        pal_timer_start(TIMER_LED_OFF,
                        500000,
                        TIMEOUT_RELATIVE,
                        (FUNC_PTR)data_exchange_led_off_cb,
                        NULL);
    }

    /* Keep compiler happy. */
    msduHandle = msduHandle;
#ifdef ENABLE_TSTAMP
    Timestamp = Timestamp;
#endif  /* ENABLE_TSTAMP */
}


/**
 * @brief Callback function switching off the LED
 */
static void data_exchange_led_off_cb(void *parameter)
{
    printf("LED off.\n\r");
    pal_led(LED_DATA, LED_OFF);

    parameter = parameter;  /* Keep compiler happy. */
}



// vim:ai:cin:sw=4 sts=4 ft=c

/* EOF */
