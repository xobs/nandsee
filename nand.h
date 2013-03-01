#ifndef __NAND_H__
#define __NAND_H__

#include <stdint.h>
uint8_t nand_unscramble_byte(uint8_t byte);
int nand_print(struct state *st, uint8_t data, uint8_t ctrl);
uint8_t nand_ale(uint8_t ctrl);
uint8_t nand_cle(uint8_t ctrl);
uint8_t nand_we(uint8_t ctrl);
uint8_t nand_re(uint8_t ctrl);
uint8_t nand_cs(uint8_t ctrl);
uint8_t nand_rb(uint8_t ctrl);

#endif // __NAND_H__
