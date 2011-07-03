#include <AVR2025.h>

#define PRIu32 "lu"
#define PRIX8 "x"

/* === INCLUDES ============================================================ */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pal.h>
#include <tal.h>
#include <mac.h>
#include <mac_api.h>
#include <app_config.h>
#include <ieee_const.h>
#include <sio_handler.h>

/* === TYPES =============================================================== */

typedef struct prom_mode_payload_tag
{
    uint16_t fcf;
    uint8_t frame_type;
    uint8_t frame_length;
    uint8_t sequence_number;
    uint8_t dest_addr_mode;
    uint16_t dest_panid;
    address_field_t dest_addr;
    uint8_t src_addr_mode;
    uint16_t src_panid;
    address_field_t src_addr;
    uint8_t ppduLinkQuality;
    uint32_t timestamp;
    uint8_t payload_length;
    uint8_t *payload_data;
} prom_mode_payload_t;

/* === MACROS ============================================================== */

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

/* === GLOBALS ============================================================= */

static uint32_t rx_frame_cnt;
static prom_mode_payload_t app_parse_data;
static uint8_t current_page;
static uint8_t current_channel;

/* === PROTOTYPES ========================================================== */

static void print_frame(uint8_t *ptr_to_msdu, uint8_t len_msdu);
static void print_start_menu(void);
static uint8_t get_channel(void);
#ifdef HIGH_DATA_RATE_SUPPORT
static uint8_t get_page(void);
#endif  /* HIGH_DATA_RATE_SUPPORT */

/* === IMPLEMENTATION ====================================================== */

void setup(void) 
{ 
  example_main(); 
}
void loop(void) 
{

}
/**
 * @brief Main function of the promiscuous mode demo application
 */
int example_main(void)
{
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
    pal_led_init();

    /*
     * The stack is initialized above, hence the global interrupts are enabled
     * here.
     */
    pal_global_irq_enable();

    /* Initialize the serial interface used for communication with terminal program */
    if (pal_sio_init(SIO_CHANNEL) != MAC_SUCCESS)
    {
        // something went wrong during initialization
        pal_alert();
    }

#if ((!defined __ICCAVR__) && (!defined __ICCARM__) && (!defined __GNUARM__) && \
     (!defined __ICCAVR32__) && (!defined __GNUAVR32__))
    fdevopen(_sio_putchar, _sio_getchar);
#endif

    /* To Make sure the Hyper Terminal to the System */
    sio_getchar();

    /*
     * Reset the MAC layer to the default values
     * This request will cause a mlme reset confirm message ->
     * usr_mlme_reset_conf
     */
    wpan_mlme_reset_req(true);

    print_start_menu();

    /* Main loop */
    while (1)
    {
        wpan_task();
    }
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
        /* Get Current Channel Page. */
        wpan_mlme_get_req(phyCurrentPage);
    }
    else
    {
        // something went wrong; restart
        wpan_mlme_reset_req(true);
    }
}





/**
 * @brief Callback function usr_mlme_set_conf
 *
 * @param status        Result of requested PIB attribute set operation
 * @param PIBAttribute  Updated PIB attribute
 */
void usr_mlme_set_conf(uint8_t status, uint8_t PIBAttribute)
{
    if ((status == MAC_SUCCESS) && (PIBAttribute == phyCurrentPage))
    {
        printf("\r\nCurrent channel page: %d\r\n", current_page);

        /* Get Current Channel. */
        wpan_mlme_get_req(phyCurrentChannel);

    }
    else if ((status == MAC_SUCCESS) && (PIBAttribute == phyCurrentChannel))
    {
        printf("\r\nCurrent channel: %d\r\n", current_channel);

        /*
         * Set RX on when idle to enable the receiver in promiscuous mode.
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
        printf("\r\nReceiver is on\r\n\r\n");
        /*
         * Set promiscuous mode.
         * Use: bool wpan_mlme_set_req(uint8_t PIBAttribute,
         *                             void *PIBAttributeValue);
         *
         * This request leads to a set confirm message -> usr_mlme_set_conf
         */
         bool promiscuous_mode = true;

         wpan_mlme_set_req(macPromiscuousMode, &promiscuous_mode);
    }
    else if ((status == MAC_SUCCESS) && (PIBAttribute == macPromiscuousMode))
    {
        printf("\r\nPromiscuous mode is on\r\n\r\n");
        /*
         * Node is now in promiscuous mode and will receive all proper frames
         * on this channel via MCPS_DATA.incidation primitives.
         */
    }
    else
    {
        printf("\r\nSetting of PIB attribute failed");
        printf("\r\nPromiscuous mode is off");
        printf("\r\nApplication stopps...\r\n\r\n");
    }
}



/**
 * @brief Callback function usr_mlme_get_conf
 *
 * @param status            Result of requested PIB attribute set operation
 * @param PIBAttribute      Updated PIB attribute
 * @param PIBAttributeValue Pointer to data containing retrieved PIB attribute
 */
void usr_mlme_get_conf(uint8_t status,
                       uint8_t PIBAttribute,
                       void *PIBAttributeValue)
{
    if ((status == MAC_SUCCESS) && (PIBAttribute == phyCurrentPage))
    {
#ifdef HIGH_DATA_RATE_SUPPORT
        printf("\r\nCurrent channel Page: %d\r\n", *(uint8_t *)PIBAttributeValue);

        /* Ask for new channel page. */
        current_page = get_page();
#else
        current_page = DEFAULT_CHANNEL_PAGE;
#endif

        wpan_mlme_set_req(phyCurrentPage, &current_page);
    }
    else if ((status == MAC_SUCCESS) && (PIBAttribute == phyCurrentChannel))
    {
        printf("\r\nCurrent channel: %d\r\n", *(uint8_t *)PIBAttributeValue);

        /* Ask for new channel. */
        current_channel = get_channel();

        wpan_mlme_set_req(phyCurrentChannel, &current_channel);
    }
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
 * @param Timestamp        The time, in symbols, at which the data were received.
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
    uint8_t *payload_ptr = msdu;

    /* Update rx counter. */
    rx_frame_cnt++;

    /*
     * The relevant information in promiscuous mode is in the payload (*msdu)
     * of this callback. This payload contains the MHR of the original frame
     * and the payload of the original frame.
     * See IEEE 802.15.4-2006 section 7.5.6.5 Promiscuous mode.
     */

    /*
     * Now start parsing the received payload to get the elements of the
     * MHR of the frame.
     */
    app_parse_data.fcf = convert_byte_array_to_16_bit(payload_ptr);

    /*
     * MAC is maintained in Little endian stack and in order to get the fcf
     * we need to convert the endianess for further check
     */
    app_parse_data.fcf = CPU_ENDIAN_TO_LE16(app_parse_data.fcf);

    app_parse_data.frame_type = FCF_GET_FRAMETYPE(app_parse_data.fcf);
    app_parse_data.frame_length = msduLength;

    app_parse_data.dest_addr_mode = FCF_GET_DEST_ADDR_MODE(app_parse_data.fcf);
    app_parse_data.src_addr_mode = FCF_GET_SOURCE_ADDR_MODE(app_parse_data.fcf);

    payload_ptr += sizeof(app_parse_data.fcf);

    if (app_parse_data.dest_addr_mode != 0)
    {
        app_parse_data.dest_panid = convert_byte_array_to_16_bit(payload_ptr);
        payload_ptr += sizeof(app_parse_data.dest_panid);

        if (FCF_SHORT_ADDR == app_parse_data.dest_addr_mode)
        {
            app_parse_data.dest_addr.short_address = convert_byte_array_to_16_bit(payload_ptr);
            payload_ptr += 2;   // Length of short address
        }
        else if (FCF_LONG_ADDR == app_parse_data.dest_addr_mode)
        {
            app_parse_data.dest_addr.long_address = convert_byte_array_to_64_bit(payload_ptr);
            payload_ptr += 8;   // Length of extended address
        }
    }

    if (app_parse_data.src_addr_mode != 0)
    {
        if (!(app_parse_data.fcf & FCF_PAN_ID_COMPRESSION))
        {
            /*
             * Source PAN ID is present in the frame only if the intra-PAN bit
             * is zero and src_address_mode is non zero.
             */
            app_parse_data.src_panid = convert_byte_array_to_16_bit(payload_ptr);
            payload_ptr += sizeof(app_parse_data.src_panid);
        }
        else
        {
            /*
             * The received frame does not contain a source PAN ID, hence
             * source PAN ID of the frame_info_t is updated with the
             * destination PAN ID.
             */
            app_parse_data.src_panid = app_parse_data.dest_panid;
        }

        /* The frame_info_t structure is updated with the source address. */
        if (FCF_SHORT_ADDR == app_parse_data.src_addr_mode)
        {
            app_parse_data.src_addr.short_address = convert_byte_array_to_16_bit(payload_ptr);
            payload_ptr += 2;   // Length of short address
        }
        else if (FCF_LONG_ADDR == app_parse_data.src_addr_mode)
        {
            app_parse_data.src_addr.long_address = convert_byte_array_to_64_bit(payload_ptr);
            payload_ptr += 8;   // Length of extended address
        }
    }

    app_parse_data.sequence_number = DSN;

    app_parse_data.payload_length = msduLength - (payload_ptr - msdu);

    app_parse_data.ppduLinkQuality = mpduLinkQuality;
#ifdef ENABLE_TSTAMP
    app_parse_data.timestamp = Timestamp;
#endif
    app_parse_data.payload_data = payload_ptr;

    /*
     * Print the currently received frame via SIO.
     * Note: For demonstration purposes only the octets of the complete payload
     * of the MCPS-DATA.indication message (containing both MAC Header and
     * payload of actual received frame) is printed.
     * The MAC Header elements are not printed separately because the SIO (via
     * UART) may be the limiting factor.
     * If the MAC Header elements are required, please refer to the elemtens of
     * variable app_parse_data.
     */
    print_frame(msdu, msduLength);

    /* Keep compiler happy. */
    SrcAddrSpec = SrcAddrSpec;
    DstAddrSpec = DstAddrSpec;
}



/**
 * @brief Parse the received MCPS-DATA.indication message for the contained frame
 */
static void print_frame(uint8_t *ptr_to_msdu, uint8_t len_msdu)
{
    char ascii[5];

    printf("No. %" PRIu32 "  ", rx_frame_cnt);

    switch (app_parse_data.frame_type)
    {
        case FCF_FRAMETYPE_BEACON:
            printf("Beacon: ");
            break;

        case FCF_FRAMETYPE_DATA:
            printf("Data: ");
            break;

        case FCF_FRAMETYPE_ACK:
            printf("Ack: ");
            break;

        case FCF_FRAMETYPE_MAC_CMD:
            printf("Cmd: ");
            break;

        default:
            printf("Unknown frame: ");
            break;
    }

    for (uint8_t i = 0; i < len_msdu; i++)
    {
        sprintf(ascii, "%.2" PRIX8 " ", *ptr_to_msdu);
        printf(ascii);
        ptr_to_msdu++;
    }

    printf("\r\n");
}



/**
 * @brief Print start menu to demo application
 */
static void print_start_menu(void)
{
    printf("\r\n\r\n************************************************************\r\n");
    printf("Promiscuous mode demo application (");

    /* Transceiver version */
#if (TAL_TYPE == AT86RF212)
    printf("AT86RF212");
#elif (TAL_TYPE == AT86RF230A)
    printf("AT86RF230A");
#elif (TAL_TYPE == AT86RF230B)
    printf("AT86RF230B");
#elif (TAL_TYPE == AT86RF231)
    printf("AT86RF231");
#elif (TAL_TYPE == AT86RF232)
    printf("AT86RF232");
#elif (TAL_TYPE == ATMEGARF_TAL_1)
    // no output
#else
#error "unknown TAL type ";
#endif

#if (TAL_TYPE != ATMEGARF_TAL_1)
    printf(" / ");
#endif

    /* Print MCU version */
#if (PAL_GENERIC_TYPE == AVR)
    #if (PAL_TYPE == ATMEGA1281)
        printf("ATmega1281");
    #elif (PAL_TYPE == ATMEGA2561)
        printf("ATmega2561");
    #elif (PAL_TYPE == ATMEGA644P)
        printf("ATmega644P");
    #elif (PAL_TYPE == ATMEGA1284P)
        printf("ATmega1284P");
    #elif (PAL_TYPE == AT90USB1287)
        printf("AT90USB1287");
    #else
    #error "unknown PAL_TYPE";
    #endif
#elif (PAL_GENERIC_TYPE == XMEGA)
    #if (PAL_TYPE == ATXMEGA128A1)
        printf("ATxmega128A1");
    #elif (PAL_TYPE == ATXMEGA256A3)
        printf("ATXMEGA256A3");
    #else
    #error "unknown PAL_TYPE";
    #endif
#elif (PAL_GENERIC_TYPE == MEGA_RF)
    #if (PAL_TYPE == ATMEGA128RFA1)
        printf("ATmega128RFA1");
    #else
    #error "unknown PAL_TYPE";
    #endif
#elif (PAL_GENERIC_TYPE == ARM7)
    #if (PAL_TYPE == AT91SAM7X256)
        printf("AT91SAM7X256");
    #else
    #error "unknown PAL_TYPE";
    #endif
#elif (PAL_GENERIC_TYPE == AVR32)
    #if (PAL_TYPE == AT32UC3A3256)
        printf("AT32UC3A3256");
    #elif (PAL_TYPE == AT32UC3L064)
        printf("AT32UC3L064");
    #elif (PAL_TYPE == AT32UC3B1128)
        printf("AT32UC3B1128");
    #else
    #error "unknown PAL_TYPE";
    #endif
#elif (PAL_GENERIC_TYPE == SAM3)
    #if (PAL_TYPE == AT91SAM3S4C)
        printf("AT91SAM3S4C");
    #elif (PAL_TYPE == AT91SAM3S4B)
        printf("AT91SAM3S4B");
    #else
    #error "unknown PAL_TYPE";
    #endif
#else
    #error "unknown PAL_GENERIC_TYPE";
#endif

    printf(")\r\n");
}



/**
 * @brief Sub-menu to get channel setting
 */
static uint8_t get_channel(void)
{
    char input_char[3]= {0, 0, 0};
    uint8_t i;
    uint8_t input;
    uint8_t channel;

#if (RF_BAND == BAND_2400)
    printf("\r\nEnter channel (11..26) and press 'Enter': \r\n");
#else
    printf("\r\nEnter channel (0..10) and press 'Enter': \r\n");
#endif
    for (i = 0; i < 3; i++)
    {
        input = sio_getchar();
        if ((input < '0') || (input > '9'))
        {
            break;
        }
        input_char[i] = input;
    }

    channel = atol(input_char);
    return (channel);
}



#ifdef HIGH_DATA_RATE_SUPPORT
/**
 * @brief Sub-menu to get channel page setting
 */
static uint8_t get_page(void)
{
#if (TAL_TYPE == AT86RF232)
    printf("\r\nOnly channel page 0 supported by AT86RF232.\r\n");
    printf("\r\nPress any key to return to main menu.\r\n");
    sio_getchar();
    return (DEFAULT_CHANNEL_PAGE);
#else
    char input_char[3]= {0, 0, 0};
    uint8_t i;
    uint8_t input;
    uint8_t ch_page;

#if (TAL_TYPE == AT86RF212)
    printf("\r\nchannel page\tbrutto data rate (kbit/s)\r\n");
    printf("page\tch 0 or \tch 1-10\r\n");
    printf("0\t20 or \t\t40 (BPSK)\r\n");
    printf("2\t100 or \t\t250 (O-QPSK)\r\n");
    printf("5\t250 (Chinese band, ch 0-3)\r\n");
    printf("16 *)\t200 or \t\t500\r\n");
    printf("17 *)\t400 or \t\t1000\r\n");
    printf("18 *)\t500 (Chinese band, ch 0-3)\r\n");
    printf("19 *)\t1000 (Chinese band, ch 0-3)\r\n");
    printf("*) proprietary channel page\r\n");
    printf("\r\nEnter channel page (0, 2, 5, 16, 17, 18 or 19) and press 'Enter': ");
#endif
#if ((TAL_TYPE == AT86RF231) || (TAL_TYPE == ATMEGARF_TAL_1))
    printf("\r\nchannel page\tbrutto data rate (kbit/s)\r\n");
    printf("0\t\t250\r\n");
    printf("2 *)\t\t500\r\n");
    printf("16 *)\t\t1000\r\n");
    printf("17 *)\t\t2000\r\n");
    printf("*) proprietary channel page\r\n");
    printf("\r\nEnter channel page (0, 2, 16, or 17) and press 'Enter': ");
#endif

    for (i = 0; i < 3; i++)
    {
        input = sio_getchar();
        if ((input < '0') || (input > '9'))
        {
            break;
        }
        input_char[i] = input;
    }

    ch_page = atoi(input_char);

    return (ch_page);
#endif  /* #if (TAL_TYPE == AT86RF232) */
}
#endif  /* HIGH_DATA_RATE_SUPPORT */

/* EOF */
// vim:ai:cin:sw=2 sts=2 ft=cpp
