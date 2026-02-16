/**************************************************************************
 * SIL (Software-In-the-Loop) version of APDB.h
 * This version removes embedded-specific syntax for standard C compilation
 **************************************************************************/
#ifndef _APDB_H
#define _APDB_H

#include "IO_Driver.h"

/* prototype for cstart */
extern void _cstart( void );

/**************************************************************************
 *
 * D E F I N I T I O N S
 *
 **************************************************************************/

// #define APPL_START      ((ubyte4) &_cstart)
#define APPL_START      0 // SIL: use 0 as placeholder for main address

#define APDB_ADDRESS    0xC10000

/* SIL version: regular variable instead of embedded-specific __at() */
#define APDB BL_APDB


/**************************************************************************
 *
 * D A T A   S T R U C T U R E
 *
 **************************************************************************/

/**
 *
 * \brief Date structure
 *
 *  Data structure for saving dates like flash or build date.
 *
 */
typedef struct _bl_t_date
{
    ubyte4 date;          /**< date.  */
}BL_T_DATE;


/**
 *
 * \brief CAN ID structure
 *
 *
 */
typedef struct _bl_t_can_id
{
    ubyte4 extended;        /**< if bit is set to 1, extended CAN ID is used    */
    ubyte4 ID;              /**< right-aligned CAN-ID, LSB must start at bit 0 */
}BL_T_CAN_ID;


/**
 *
 * \brief APDB structure
 *
 *  Data structure for accessing the Application Descriptor Block.
 *
 */
typedef struct _bl_apdb
{
    ubyte4 versionAPDB;         /**< [8bit.8bit] version of the APDB           */
    BL_T_DATE flashDate;        /**< date when the application has
                                     been flashed.                             */
    BL_T_DATE buildDate;        /**< date when the application has
                                     been built (only if used by the customer) */
    ubyte4 nodeType;            /**< Hardware type (\ttc50, \ttc60, ..)
                                     To match software to hardware in PC-Tool  */
    ubyte4 startAddress;        /**< start address: address where the code
                                     in the memory starts                      */
    ubyte4 codeSize;            /**< size of the application in bytes          */
    ubyte4 legacyAppCRC;        /**< legacy application crc for flash checker  */
    ubyte4 appCRC;              /**< CRC-32 of the application                 */
    ubyte4 nodeNr;              /**< node number                               */
    ubyte4 CRCInit;             /**< init value for certain CRC checks         */
    ubyte4 flags;               /**< various flags                             */
    ubyte4 hook1;               /**< custom hook 1                             */
    ubyte4 hook2;               /**< custom hook 2                             */
    ubyte4 hook3;               /**< custom hook 3                             */
    ubyte4 mainAddress;         /**< vector address of the application, main() */
    BL_T_CAN_ID canDownloadID;  /**< CAN ID for download                       */
    BL_T_CAN_ID canUploadID;    /**< CAN ID for upload                         */
    ubyte4 legacyHeaderCRC;     /**< legacy header crc for flash checker       */
    ubyte4 version;             /**< [8bit.8bit.16bit] version of the application
                                     (only if used by the customer)            */
    ubyte4 canBaudrate;         /**< baud rate for CAN communication [kbit/s]  */
    ubyte4 canChannel;          /**< channel for CAN communication             */
    ubyte4 password;            /**< hash-value of password for memory access  */
    ubyte4 magicSeed;           /**< Initialise seed value for CRC calculation
                                     with the MCHK HW module                   */
    ubyte1 reserved[6*4];       /**< reserved for future use                   */
    ubyte4 headerCRC;           /**< CRC-32 of the header section
                                     (including APPCRC)                        */
}BL_APDB;


#endif /* _APDB_H */
