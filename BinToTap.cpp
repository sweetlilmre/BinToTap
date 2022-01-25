
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>

/* half-wave pulse timer values */
#define CBMPULSE_SHORT (0x30 - 1)
#define CBMPULSE_MEDIUM (0x42 - 1)
#define CBMPULSE_LONG (0x56 - 1)

uint32_t tapDataLen = 0;

struct tapHeader
{
    char magic[12];
    uint8_t version;
    uint8_t platform;
    uint8_t video;
    uint8_t reserved;
    uint32_t dataLen;
};

char TAP_FILE_NAME16[16] = {'T', 'A', 'P', 'E', ' ', 'C', 'A', 'R', 'T', ' ', 'L', 'O', 'A', 'D', 'E', 'R'};
static const uint8_t default_loader[171] = {
    /* auto-generated from loader.bin on Fri Dec 31 10:33:12 2021 */
    0x6e, 0x11, 0xd0, 0x78, 0x18, 0xa0, 0x10, 0xa2, 0x00, 0x86, 0xc6, 0xa9, 0x27, 0x2e, 0xf7, 0x03, 0x90, 0x02, 0x09, 0x08, 0x85, 0x01, 0xca, 0xea, 0xd0, 0xfc, 0x29, 0xdf, 0x85,
    0x01, 0xca, 0xd0, 0xfd, 0x88, 0xd0, 0xe7, 0xa9, 0x30, 0x85, 0x01, 0xa0, 0xfa, 0x20, 0xb6, 0x03, 0x99, 0xb0, 0xff, 0xc8, 0xd0, 0xf7, 0x20, 0xb6, 0x03, 0x91, 0xae, 0xe6, 0xae,
    0xd0, 0x02, 0xe6, 0xaf, 0xa5, 0xae, 0xc5, 0xac, 0xd0, 0xef, 0xa6, 0xaf, 0xe4, 0xad, 0xd0, 0xe9, 0xa0, 0x37, 0x84, 0x01, 0x38, 0x2e, 0x11, 0xd0, 0x86, 0x2e, 0x86, 0x30, 0x86,
    0x32, 0x85, 0x2d, 0x85, 0x2f, 0x85, 0x31, 0x58, 0x20, 0x53, 0xe4, 0x6c, 0xaa, 0x00, 0xa9, 0x10, 0x24, 0x01, 0xd0, 0xfc, 0xa2, 0x38, 0xa9, 0x27, 0x86, 0x01, 0x85, 0x00, 0xea,
    0xa5, 0x01, 0x29, 0x18, 0x4a, 0x4a, 0x45, 0x01, 0x4a, 0x29, 0x0f, 0xaa, 0xa5, 0x01, 0x29, 0x18, 0x4a, 0x4a, 0x45, 0x01, 0x4a, 0x29, 0x0f, 0x1d, 0xe7, 0x03, 0xa2, 0x2f, 0x86,
    0x00, 0xe8, 0x86, 0x01, 0x60, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0xca, 0x00, 0x00, 0x00, 0x00

};

static uint8_t header_loader[171] = {0};
static uint8_t start_addr_low = 0x02, start_addr_high = 0x03;
static uint8_t end_addr_low = 0x04, end_addr_high = 0x03;

#define DEFAULT_TAPNAME "TapeCartLoader"
#define HEADER_TAPNAME "HeaderLoader"
char tapfileName[256] = {0};

FILE* tapFile = NULL;

/* ====================================================================== */
/* = CBM tape format generator                                          = */
/* ====================================================================== */

/* add a single pulse in the buffer, waiting for free space if req'd */
static void put_pulse(uint8_t pulse)
{
    /* calculate index of next entry */
    fwrite(&pulse, 1, 1, tapFile);
    tapDataLen++;
}

static void cbm_sync(uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
        put_pulse(CBMPULSE_SHORT);
}

static void cbm_bit(uint8_t value)
{
    if (value)
    {
        put_pulse(CBMPULSE_MEDIUM);
        put_pulse(CBMPULSE_SHORT);
    }
    else
    {
        put_pulse(CBMPULSE_SHORT);
        put_pulse(CBMPULSE_MEDIUM);
    }
}

static void cbm_byte(uint8_t value)
{
    /* marker */
    put_pulse(CBMPULSE_LONG);
    put_pulse(CBMPULSE_MEDIUM);

    bool parity = true;
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t bit = value & 1;

        cbm_bit(bit);
        if (bit)
            parity = !parity;

        value >>= 1;
    }

    cbm_bit(parity);
}

/* data function to transmit a header block */
static void header_datafunc(void* arg)
{
    /* transmit leader */
    uint8_t checksum = 0x03 ^ start_addr_low ^ start_addr_high ^ end_addr_low ^ end_addr_high;
    cbm_byte(0x03);           // load to absolute address
    cbm_byte(start_addr_low); // start address
    cbm_byte(start_addr_high);
    cbm_byte(end_addr_low); // end address + 1
    cbm_byte(end_addr_high);

    /* transmit file name */
    for (uint8_t i = 0; i < 16; i++)
    {
        uint8_t b = TAP_FILE_NAME16[i];
        cbm_byte(b);
        checksum ^= b;
    }

    /* transmit remainder of loader */

    for (uint8_t i = 0; i < 192 - 16 - 5; i++)
    {
        uint8_t b = header_loader[i];
        cbm_byte(b);
        checksum ^= b;
    }

    cbm_byte(checksum);
}

/* data function to transmit a start vector */
static void vector_datafunc(void* arg)
{
    cbm_byte(0x51);
    cbm_byte(0x03);
    cbm_byte(0x51 ^ 0x03);
}

/* data function to transmit a start vector */
static void prg_datafunc(void* arg)
{
    FILE* prgFile = (FILE*) arg;
    // this will be called twice, reset the filepointer to after the PRG load address
    fseek(prgFile, 2, SEEK_SET);
    uint8_t checksum = 0;
    uint8_t data;
    while (fread(&data, 1, 1, prgFile) == 1)
    {
        cbm_byte(data);
        checksum ^= data;
    }
    cbm_byte(checksum);
}

/* transmit a data block */
static void cbm_datablock(void (*datafunc)(void*), void* arg = NULL)
{
    for (uint8_t repeat = 0; repeat < 2; repeat++)
    {
        /* send countdown sequence */
        for (uint8_t i = 9; i > 0; i--)
        {
            if (repeat)
                cbm_byte(i);
            else
                cbm_byte(i + 0x80);
        }

        datafunc(arg);

        /* transmit block gap */
        put_pulse(CBMPULSE_LONG);
        cbm_sync(60); // note: should be less than the tap_buffer size
    }
}

void writeTapHeader(FILE* tapFile, uint32_t dataLen)
{
    tapHeader hdr;
    memcpy(hdr.magic, "C64-TAPE-RAW", 12);
    hdr.version = 1;
    hdr.platform = 0;
    hdr.video = 0;
    hdr.reserved = 0;
    hdr.dataLen = dataLen;
    fseek(tapFile, 0, SEEK_SET);
    fwrite(&hdr, sizeof(hdr), 1, tapFile);
}

void writeTap(void (*datafunc)(void*), void* arg = 0)
{
    tapFile = fopen(tapfileName, "wb");
    if (tapFile != NULL)
    {
        tapDataLen = 0;
        writeTapHeader(tapFile, 0);

        /* transmit header block */
        // http://www.sleepingelephant.com/ipw-web/bulletin/bb/viewtopic.php?f=14&t=10027#p112805 suggests that 1500 should be ok
        cbm_sync(1500); // note: 500 appears to be less reliable
        cbm_datablock(header_datafunc);

        /* transmit the autostart vector */
        cbm_sync(1500); // note: 500 does not work
        cbm_datablock(datafunc, arg);

        writeTapHeader(tapFile, tapDataLen);

        fclose(tapFile);
    }
    else
    {
        printf("Unable to create tap file: %s!\n", tapfileName);
    }
}

long getFileSize(const char* fileName)
{
    struct stat stat_buf;
    int rc = stat(fileName, &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void setTapName(const char* baseName)
{
    memset(tapfileName, 0, 256);
    strncpy(tapfileName, baseName, 256 - 1 - 4);
    strcat(tapfileName, ".tap");
}

void printUsage()
{
    printf("USAGE:\n");
    printf("\n");
    printf("BinToTap args [input file]\n");
    printf("  BinToTap -d\n");
    printf("    Will generate a TAP file containting the default TAPECART fastloader from Ingo Korb\n");
    printf("    The file will be named %s\n\n", DEFAULT_TAPNAME);
    printf("  BinToTap -h loaderfile\n");
    printf("    Will generate a TAP file containting the loader specified by \"loaderfile\" this must be exactly 171 bytes in size\n");
    printf("    The file will be named %s\n\n", HEADER_TAPNAME);
    printf("  BinToTap -p prgfile\n");
    printf("    Will generate a TAP file containting the PRG specified by \"prgfile\"\n");
    printf("    The file will be named the same as the PRG file, but with a TAP extension\n\n");
    printf("  If -n loadname is specified with any other option the \"loadname\" string (max 16 chars) will be used in the tape header\n");
}

bool parseArgs(int argc, char* argv[])
{
    // make sure we init to zero for padding.
    memset(header_loader, 0, 171);

    int opt;
    while ((opt = getopt(argc, argv, "dn:h:p:")) != -1)
    {
        switch (opt)
        {
            case 'n':
            {
                int len = strlen(optarg);
                if (len > 16)
                {
                    printf("-n specified with a string longer than 16 characters!\n");
                    printf("It will be truncated\n");
                }
                for (int i = 0; i < 16; i++)
                {
                    char c = i < len ? optarg[i] : ' ';
                    if (c >= 'a' && c <= 'z')
                        c = c - 32;
                    if (c < 32 || c > 95)
                        c = ' ';
                    TAP_FILE_NAME16[i] = c;
                }
            }
            break;
            case 'd':
            {
                setTapName(DEFAULT_TAPNAME);
                printf("Writing default loader to %s\n", tapfileName);

                memcpy(header_loader, default_loader, 171);
                writeTap(vector_datafunc);
            }
            break;
            case 'h':
            {
                const char* loader_arg = optarg;
                setTapName(HEADER_TAPNAME);
                printf("Writing custom loader %s to %s\n", loader_arg, tapfileName);

                FILE* loaderFile = fopen(loader_arg, "r+b");
                if (!loaderFile)
                {
                    printf("Unable to open loaderfile: %s\n", loader_arg);
                    return false;
                }
                long read = fread(header_loader, 1, 171, loaderFile);
                if (read != 171)
                {
                    printf("loaderfile size was: %ld not exactly 171 bytes long!\n", read);
                    if (read < 171)
                    {
                        printf("Missing bytes are zero padded!\n");
                    }
                    else
                    {
                        printf("Extra bytes ignored!\n");
                    }
                    printf("Bad things may happen!\n");
                }

                writeTap(vector_datafunc);
                fclose(loaderFile);
            }
            break;
            case 'p':
            {
                const char* prg_arg = optarg;
                setTapName(prg_arg);

                printf("Writing tap from %s to %s\n", prg_arg, tapfileName);
                FILE* prgFile = fopen(prg_arg, "rb");
                if (!prgFile)
                {
                    printf("Unable to open prgfile: %s\n", prg_arg);
                    return false;
                }

                long fileSize = getFileSize(prg_arg);
                fread(&start_addr_low, 1, 1, prgFile);
                fread(&start_addr_high, 1, 1, prgFile);
                fileSize -= 2;
                uint16_t start_addr = start_addr_low | (start_addr_high << 8);
                uint16_t end_addr = start_addr + fileSize;
                end_addr_low = end_addr & 0xFF;
                end_addr_high = end_addr >> 8;

                writeTap(prg_datafunc, prgFile);

                fclose(prgFile);
            }
            break;
            case '?':
            {
                printUsage();
            }
            break;
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
    printf("BinToTap v1.0\n");
    for (int i = 0; i < argc; i++)
    {
        printf("arg: %d, val: %s\n", i, argv[i]);
    }
    if (argc == 1)
    {
        printUsage();
        return (0);
    }
    if (!parseArgs(argc, argv))
        return 1;
}
