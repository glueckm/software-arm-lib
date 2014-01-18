/*
 *  bus.h - Low level EIB bus access.
 *
 *  Copyright (c) 2014 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */
#ifndef sblib_bus_h
#define sblib_bus_h

#include <sblib/core.h>
#include <sblib/eib/bcu_type.h>

class Bus;

/**
 * The EIB bus access object.
 */
extern Bus bus;


// The state of the lib's telegram sending/receiving
enum SbState
{
    // The lib is idle. No receiving or sending.
    SB_IDLE = 0,

    // The lib is receiving a byte.
    SB_RECV_BYTE,

    // The lib is waiting for the start bit of the next byte.
    SB_RECV_START,

    // Start sending the telegram in sbSendTelegram[].
    SB_SEND_INIT,

    // Start sending the next byte of a telegram
    SB_SEND_START,

    // Send the first bit of the current byte
    SB_SEND_BIT_0,

    // Send the bits of the current byte
    SB_SEND_BYTE,

    // Wait between two sendings
    SB_SEND_WAIT,

    // Finish sending
    SB_SEND_END
};


/**
 * Bus short acknowledgment frame: acknowledged
 */
#define SB_BUS_ACK 0xcc

/**
 * Bus short acknowledgment frame: not acknowledged
 */
#define SB_BUS_NACK 0x0c

/**
 * Bus short acknowledgment frame: busy
 */
#define SB_BUS_BUSY 0xc0

/**
 * Bus short acknowledgment frame: not acknowledged & busy
 * Shall be handled as SB_BUS_BUSY
 */
#define SB_BUS_NACK_BUSY 0x00


// The size of the telegram buffer in bytes
#define SB_TELEGRAM_SIZE 24

/**
 * The telegram that is currently being sent.
 */
extern unsigned char *sendCurTelegram;

/**
 * The telegram to be sent after sbSendTelegram is done.
 */
extern unsigned char *sbSendNextTelegram;

/**
 * Send an acknowledge or not-acknowledge byte if != 0
 */
extern unsigned char sendAck;

/**
 * Test if we are in programming mode (the button on the controller is pressed and
 * the red programming LED is on).
 *
 * @return 1 if in programming mode, 0 if not.
 */
#define sb_prog_mode_active() (sbUserRam->status & 1)


/**
 * Low level class for EIB bus access.
 *
 * When creating a bus object, the handler for the timer must also be
 * created. Example:
 *
 * Bus mybus(timer32_0);
 * BUS_TIMER_INTERRUPT_HANDLER(TIMER32_0_IRQHandler, mybus);
 */
class Bus
{
public:
    /**
     * Create a bus access object.
     *
     * @param timer - The timer to use.
     * @param rxPin - The pin for receiving from the bus, e.g. PIO1_8
     * @param txPin - The pin for sending to the bus, e.g. PIO1_10
     * @param captureChannel - the timer capture channel of rxPin, e.g. CAP0
     * @param matchChannel - the timer match channel of txPin, e.g. MAT0
     */
    Bus(Timer& timer, int rxPin, int txPin, byte captureChannel, byte matchChannel);

    /**
     * Begin using the bus.
     *
     * This powers on all used components.
     * This method must be called before the bus can be used.
     */
    void begin();

    /**
     * End using the bus.
     *
     * This powers the bus off.
     */
    void end();

    /**
     * Test if the bus is idle, no telegram is about to being sent or received.
     *
     * @return true when idle, false when not.
     */
    bool idle() const;

    /**
     * Send a telegram. The checksum byte will be added at the end of telegram[].
     * Ensure that there is at least one byte space at the end of telegram[].
     *
     * @param telegram - the telegram to be sent.
     * @param length - the length of the telegram in sbSendTelegram[], without the checksum
     */
    void sendTelegram(unsigned char* telegram, unsigned short length);

    /**
     * This method is called by the timer interrupt handler.
     * Consider it to be a private method and do not call it.
     */
    void timerInterruptHandler();

    /**
     * Test if there is a telegram being sent.
     *
     * @return True if there is a telegram to be sent, false if not.
     */
    bool sendingTelegram() const;

    /**
     * Test if there is a received telegram in bus.telegram[].
     *
     * @return True if there is a telegram in bus.telegram[], false if not.
     */
    bool telegramReceived() const;

    /**
     * Discard the received telegram. Call this method when you successfully
     * processed the telegram.
     */
    void discardReceivedTelegram();

    /** The state of the telegram sending/receiving */
    enum State
    {
        IDLE,                //!< The lib is idle. No receiving or sending.
        RECV_BYTE,           //!< The lib is receiving a byte.
        RECV_START,          //!< The lib is waiting for the start bit of the next byte.
        SEND_INIT,           //!< Start sending the telegram in sbSendTelegram[].
        SEND_START,          //!< Start sending the next byte of a telegram
        SEND_BIT_0,          //!< Send the first bit of the current byte
        SEND_BYTE,           //!< Send the bits of the current byte
        SEND_WAIT,           //!< Wait between two sendings
        SEND_END             //!< Finish sending
    };

    enum
    {
        TELEGRAM_SIZE = 24   //!< The maximum size of a telegram including the checksum byte
    };

    /**
     * The received telegram.
     */
    byte telegram[TELEGRAM_SIZE];

    /**
     * The total length of the received telegram in telegram[].
     */
    byte telegramLen;

private:
    /**
     * Switch to idle state
     */
    void idleState();

    /**
     * Prepare the telegram for sending. Set the sender address to our own
     * address, and calculate the checksum of the telegram.
     * Stores the checksum at telegram[length].
     *
     * @param telegram - the telegram to process
     * @param length - the length of the telegram
     */
    void prepareTelegram(unsigned char* telegram, unsigned short length) const;

    /**
     * Handle the received bytes on a low level. This function is called by
     * the function TIMER16_1_IRQHandler() to decide about further processing of the
     * received bytes.
     *
     * @param valid - 1 if all bytes had correct parity and the checksum is correct, 0 if not
     */
    void handleTelegram(bool valid);

protected:
    friend class BCU;
    byte sendAck;            //!< Send an acknowledge or not-acknowledge byte if != 0
    unsigned short ownAddr;  //!< Our own physical address on the bus

private:
    byte state;              //!< The state of the lib's telegram sending/receiving
    byte sbNextByte;         //!< The number of the next byte in the telegram
    byte sendTries;          //!< The number of repeats when sending a telegram

    Timer& timer;            //!< The timer
    int currentByte;         //!< The current byte that is received/sent, including the parity bit

    // The size of the to be sent telegram in bytes (including the checksum).
    unsigned short sendTelegramLen;

    // The telegram that is currently being sent.
    unsigned char *sendCurTelegram;

    // The telegram to be sent after sbSendTelegram is done.
    unsigned char *sbSendNextTelegram;
};


/**
 * Create an interrupt handler for the EIB bus access. This macro must be used
 * once for every Bus object that is created. For the default bus object, this is
 * done.
 *
 * @param handler - the name of the interrupt handler, e.g. TIMER16_0_IRQHandler
 * @param busObj - the bus object that shall receive the interrupt.
 */
#define BUS_TIMER_INTERRUPT_HANDLER(handler, busObj) \
    extern "C" void handler() { busObj.timerInterruptHandler(); }

/**
 * Get the size of a telegram, including the protocol header but excluding
 * the checksum byte.
 *
 * @param telegram - the telegram to query
 *
 * @return The size of the telegram, excluding the checksum byte.
 */
#define telegramSize(tel) (7 + (tel[5] & 15))


//
//  Inline functions
//

inline bool Bus::idle() const
{
    return state == SB_IDLE && sendCurTelegram == 0;
}

inline bool Bus::sendingTelegram() const
{
    return sendCurTelegram != 0;
}

inline bool Bus::telegramReceived() const
{
    return telegramLen != 0;
}

inline void Bus::discardReceivedTelegram()
{
    telegramLen = 0;
}

inline void Bus::end()
{
}

#endif /*sblib_bus_h*/
