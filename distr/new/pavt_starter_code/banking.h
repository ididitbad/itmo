/**
 * @file     banking.h
 * @Author   Michael Kosyakov and Evgeniy Ivanov (ifmo.distributedclass@gmail.com)
 * @date     March, 2014
 * @brief    Definitions of data structures and functions related to banking
 *
 * Students must not modify this file!
 */

#ifndef __IFMO_DISTRIBUTED_CLASS_BANKING__H
#define __IFMO_DISTRIBUTED_CLASS_BANKING__H

#include "ipc.h"

typedef int16_t balance_t;

/**
 * 1. "Main process" sends TransferOrder to process with id=s_src.
 * 2. s_src decreases its balance by s_amount and sends TransferOrder to s_dst.
 * 3. s_dst increases its balance by s_amount.
 * 4. s_dst sends ACK to "main process".
 */
typedef struct {
    local_id   s_src;           ///< transfer from process with this ID
    local_id   s_dst;           ///< transfer to process with this ID
    balance_t  s_amount;        ///< $$$
} __attribute__((packed)) TransferOrder;

typedef struct {
    balance_t   s_balance;
    timestamp_t s_time;               ///< UNUSED physical time in PA2 or Lamport's scalar
                                      ///< time in PA3
    balance_t   s_balance_pending_in; ///< UNUSED $$$ sent at t <= s_time, but
                                      ///< received at t > s_time. PA3 only,
                                      ///< in other labs must be 0
                                      /*unused in vector-time*/
    timestamp_t s_timevector[]; //time for vector clock
} __attribute__((packed)) BalanceState;

enum {
    MAX_T = 255 ///< max possible value of timestamp returned by get_lamport_time()
                ///< or get_physical_time()
};

//------------------------------------------------------------------------------
// Functions below must be implemented by students
//------------------------------------------------------------------------------

/** Transfer amount from src to dst.
 *
 * @param parent_data Any data structure implemented by students to perform I/O
 */
void transfer(void * parent_data, local_id src, local_id dst, balance_t amount);

//------------------------------------------------------------------------------
// Functions below are implemented by lector, test implementations are
// provided to students for testing purposes
//------------------------------------------------------------------------------

/** Perform a number of transfers between various children with ids [1;max_id]
 *
 * @param parent_data   Any data structure implemented by students to perform I/O,
 *                      will be passed to transfer()
 * @param max_id max    id of existing process, so that (max_id + 1) is the total
 *                      number of processes
 */
void bank_robbery(void * parent_data, local_id max_id);

/* 
 * Returns vector time
 *
 */

 timestamp_t get_vector_time();

#endif // __IFMO_DISTRIBUTED_CLASS_BANKING__H
